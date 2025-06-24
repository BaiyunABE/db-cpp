#include "bptree.h"

class bptree::bpnode {
public:
	uint8_t node_type;			// 节点类型标记
	uint8_t key_cnt;			// 当前键数量 
	uint8_t reserved[6];		// 对齐填充
	uint64_t keys[ORDER];		// 键数组
	uint64_t children[ORDER];	// 子节点指针（内部节点）
	uint64_t next_leaf;			// 下一个叶子节点指针
	bpnode() = default;
	bpnode(uint8_t node_type, uint8_t key_cnt = 0) : node_type(node_type), key_cnt(key_cnt) { ; }
};

void bptree::init_index_header() {
	index_header.root_offset = sizeof index_header;
	index_header.tree_height = 0;
	index_header.node_cnt = 0;
}

bptree::bptree(const std::string& filename) {
	index_filename = filename + ".idx";
	index_file.open(index_filename, std::ios::in | std::ios::out | std::ios::binary);
	if (!index_file.is_open()) { // 文件不存在
		index_file.clear();
		std::ofstream tmp(index_filename, std::ios::out | std::ios::binary);
		tmp.close();
		index_file.open(index_filename, std::ios::in | std::ios::out | std::ios::binary);
		init_index_header();
	}
	else
		index_file.read(reinterpret_cast<char*>(&index_header), sizeof index_header);

	data_filename = filename + ".dat";
	data_file.open(data_filename, std::ios::in | std::ios::out | std::ios::binary);
	if (!data_file.is_open()) { // 文件不存在
		data_file.clear();
		std::ofstream tmp(data_filename, std::ios::out | std::ios::binary);
		tmp.close();
		data_file.open(data_filename, std::ios::in | std::ios::out | std::ios::binary);
	}
}

void bptree::update_index_header() {
	index_file.seekp(0x0);
	index_file.write(reinterpret_cast<char*>(&index_header), sizeof index_header);
	index_file.flush();
}

void bptree::read_index_node(bpnode* node, uint64_t offset) {
	index_file.seekg(offset);
	index_file.read(reinterpret_cast<char*>(node), sizeof * node);
}

std::string bptree::read_data(uint64_t offset) { // size 不包含 '\0'
	data_file.seekg(offset);
	uint64_t size;
	data_file.read(reinterpret_cast<char*>(&size), sizeof size);
	char* data = new char[size + 1];
	data_file.read(reinterpret_cast<char*>(data), size + 1);
	std::string res = data;
	delete[] data;
	return res;
}

void bptree::update_index_node(bpnode node, uint64_t offset) {
	index_file.seekp(offset);
	index_file.write(reinterpret_cast<char*>(&node), sizeof node);
	index_file.flush();
}

uint64_t bptree::alloc_index_node(bpnode node) {
	uint64_t offset = sizeof index_header + NODE_SIZE * index_header.node_cnt;
	update_index_node(node, offset);
	index_header.node_cnt++;
	return offset;
}

uint64_t bptree::alloc_data(const char* data, uint64_t size) { // size 不包含 '\0'
	data_file.seekp(0, std::ios::end);
	uint64_t offset = data_file.tellp();
	data_file.write(reinterpret_cast<char*>(&size), sizeof size);
	data_file.write(data, size);
	data_file.write("\0", 1);
	data_file.flush();
	return offset;
}

bool bptree::update_data(uint64_t offset, const char* data, uint64_t size) { // size 不包含 '\0'
	data_file.seekg(offset);
	uint64_t cap;
	data_file.read(reinterpret_cast<char*>(&cap), sizeof cap);
	if (cap < size)
		return false;	// 更新失败，容量不足
	data_file.seekp(offset);
	data_file.write(reinterpret_cast<char*>(&size), sizeof size);
	data_file.write(data, size);
	data_file.write("\0", 1);
	data_file.flush();
	return true;
}

void bptree::split_ith_child(uint64_t offset, int i) {
	bpnode parent, left;
	read_index_node(&parent, offset);
	read_index_node(&left, parent.children[i]);
	// set right
	bpnode right(left.node_type, ORDER / 2);
	for (int j = 0; j < ORDER / 2; j++)
		right.keys[j] = left.keys[j + ORDER / 2];
	for (int j = 0; j < ORDER / 2; j++)
		right.children[j] = left.children[j + ORDER / 2];
	if (left.node_type == 0x02) // leaf
		right.next_leaf = left.next_leaf;
	// set left
	left.key_cnt = ORDER / 2;
	// set p
	for (int j = parent.key_cnt - 1; j > i; j--)
		parent.children[j + 1] = parent.children[j];
	parent.children[i + 1] = alloc_index_node(right);
	if (left.node_type == 0x02) // leaf
		left.next_leaf = parent.children[i + 1];
	for (int j = parent.key_cnt - 1; j >= i; j--)
		parent.keys[j + 1] = parent.keys[j];
	parent.keys[i] = left.keys[ORDER / 2 - 1];
	parent.key_cnt++;
	update_index_node(parent, offset);
	update_index_node(left, parent.children[i]);
	update_index_node(right, parent.children[i + 1]);
}

bool bptree::insert_nonfull(uint64_t offset, uint64_t key, std::string data) {
	// read node
	bpnode root;
	read_index_node(&root, offset);
	// insert
	if (root.node_type == 0x02) { // root is leaf
		for (int i = 0; i < root.key_cnt; i++)
			if (root.keys[i] == key)
				return false;
		int i;
		for (i = root.key_cnt - 1; i >= 0 && key < root.keys[i]; i--)
			root.keys[i + 1] = root.keys[i];
		for (i = root.key_cnt - 1; i >= 0 && key < root.keys[i]; i--)
			root.children[i + 1] = root.children[i];
		root.keys[i + 1] = key;
		root.children[i + 1] = alloc_data(data.c_str(), data.size());
		root.key_cnt++;
		update_index_node(root, offset);
		return true;
	}
	else { // root is brunch
		int i;
		for (i = 0; i < root.key_cnt && key > root.keys[i]; i++);
		if (i == root.key_cnt) {
			i--;
			root.keys[i] = key;
			update_index_node(root, offset);
		}
		bpnode node;
		read_index_node(&node, root.children[i]);
		if (node.key_cnt == ORDER) {
			split_ith_child(offset, i);
			read_index_node(&root, offset);
			if (key > root.keys[i])
				i++;
		}
		return insert_nonfull(root.children[i], key, data);
	}
}

bool bptree::insert(uint64_t key, std::string data) {
	if (index_header.tree_height == 0) { // B+ 树为空
		bpnode root(0x02, 1); // leaf
		root.keys[0] = key;
		root.next_leaf = 0x0; // null
		root.children[0] = alloc_data(data.c_str(), data.size());
		alloc_index_node(root);
		index_header.tree_height++;
		return true;
	}
	else {
		bpnode root;
		read_index_node(&root, index_header.root_offset);
		if (root.key_cnt == ORDER) { // root is full
			bpnode parent;
			parent.node_type = 0x01; // brunch
			parent.key_cnt = 1;
			parent.keys[0] = root.keys[ORDER - 1];
			parent.children[0] = index_header.root_offset;
			index_header.root_offset = alloc_index_node(parent);
			split_ith_child(index_header.root_offset, 0);
			index_header.tree_height++;
		}
		return insert_nonfull(index_header.root_offset, key, data);
	}
}

uint64_t bptree::find_recursive(uint64_t key, uint64_t offset) {
	bpnode root;
	read_index_node(&root, offset);
	if (root.node_type == 0x01) { // brunch
		int i;
		for (i = 0; i < root.key_cnt && key > root.keys[i]; i++);
		if (i == root.key_cnt)
			return 0xffffffffffffffff;
		else
			return find_recursive(key, root.children[i]);
	}
	else { // leaf
		int i;
		for (i = 0; i < root.key_cnt && key != root.keys[i]; i++);
		if (i < root.key_cnt)
			return root.children[i];
		else
			return 0xffffffffffffffff;
	}
}

std::string bptree::find(uint64_t key) {
	if (index_header.tree_height == 0) // B+ 树为空
		return "null";
	else {
		uint64_t offset = find_recursive(key, index_header.root_offset);
		if (offset == 0xffffffffffffffff)
			return "null";
		else
			return read_data(offset);
	}
}

uint64_t bptree::find_index_node_recursive(uint64_t key, uint64_t offset) {
	bpnode root;
	read_index_node(&root, offset);
	if (key > root.keys[root.key_cnt - 1])
		return 0xffffffffffffffff;
	if (root.node_type == 0x01) { // brunch
		int i;
		for (i = 0; i < root.key_cnt && key > root.keys[i]; i++);
		return find_index_node_recursive(key, root.children[i]);
	}
	else // leaf
		return offset;
}

std::vector<std::string> bptree::find_range(uint64_t left, uint64_t right) {
	std::vector<std::string> res;
	if (index_header.tree_height == 0) // B+ 树为空
		return res;
	else {
		uint64_t offset = find_index_node_recursive(left, index_header.root_offset);
		if (offset == 0xffffffffffffffff)
			return res;
		else {
			bpnode leaf;
			do {
				read_index_node(&leaf, offset);
				for (int i = 0; i < leaf.key_cnt; i++)
					if (leaf.keys[i] >= left && leaf.keys[i] < right)
						res.push_back(read_data(leaf.children[i]));
				offset = leaf.next_leaf;
			} while (leaf.keys[leaf.key_cnt - 1] < right && offset != 0x0);
			return res;
		}
	}
}

void bptree::free_index_node(uint64_t offset) {
	// 暂不实现，通过维护时定期刷新来释放空间
}

void bptree::merge_child(uint64_t offset, int i) {
	bpnode root, left, right;
	read_index_node(&root, offset);
	read_index_node(&left, root.children[i]);
	read_index_node(&right, root.children[i + 1]);
	// set left
	for (int j = 0; j < ORDER / 2; j++)
		left.keys[j + ORDER / 2] = right.keys[j];
	for (int j = 0; j < ORDER / 2; j++)
		left.children[j + ORDER / 2] = right.children[j];
	left.key_cnt = ORDER;
	update_index_node(left, root.children[i]);
	// set right
	free_index_node(root.children[i + 1]);
	// set root
	root.key_cnt--;
	for (int j = i; j < root.key_cnt; j++)
		root.keys[j] = root.keys[j + 1];
	for (int j = i + 1; j < root.key_cnt; j++)
		root.children[j] = root.children[j + 1];
	update_index_node(root, offset);
}

int bptree::find_idx(bpnode& node, uint64_t key) {
	int i;
	for (i = 0; i < node.key_cnt; i++)
		if (node.keys[i] >= key)
			break;
	return i;
}

bool bptree::erase_nonunderflow(uint64_t offset, uint64_t key) {
	bpnode root;
	read_index_node(&root, offset);
	int i = find_idx(root, key);
	if (i >= root.key_cnt)
		return false;
	else if (root.node_type == 0x02) { // leaf
		if (root.keys[i] != key)
			return false;
		else {
			root.key_cnt--;
			for (int j = i; j < root.key_cnt; j++)
				root.keys[j] = root.keys[j + 1];
			for (int j = i; j < root.key_cnt; j++)
				root.children[j] = root.children[j + 1];
			update_index_node(root, offset);
			return true;
		}
	}
	else { // brunch
		bpnode node;
		read_index_node(&node, root.children[i]);
		if (node.key_cnt == ORDER / 2) { // underflow
			bool underflow = true;
			if (i > 0) { // left exist
				bpnode left;
				read_index_node(&left, root.children[i - 1]);
				if (left.key_cnt != ORDER / 2) { // left is not underflow
					// set node
					for (int j = ORDER / 2; j > 0; j--)
						node.keys[j] = node.keys[j - 1];
					node.keys[0] = left.keys[left.key_cnt - 1];
					for (int j = ORDER / 2; j > 0; j--)
						node.children[j] = node.children[j - 1];
					node.children[0] = left.children[left.key_cnt - 1];
					if (node.node_type == 0x01)   // leaf
						node.children[0] = left.children[left.key_cnt - 1];
					node.key_cnt++;
					update_index_node(node, root.children[i]);
					// set left
					left.key_cnt--;
					update_index_node(left, root.children[i - 1]);
					// set root
					root.keys[i - 1] = left.keys[left.key_cnt - 1];
					update_index_node(root, offset);
					underflow = false;
				}
			}
			if (underflow && i < root.key_cnt - 1) {
				bpnode right;
				read_index_node(&right, root.children[i + 1]);
				if (right.key_cnt != ORDER / 2) { // right is not underflow
					// set node
					node.keys[node.key_cnt] = right.keys[0];
					node.children[node.key_cnt] = right.children[0];
					node.key_cnt++;
					update_index_node(node, root.children[i]);
					// set right
					right.key_cnt--;
					for (int j = 0; j < right.key_cnt; j++)
						right.keys[j] = right.keys[j + 1];
					for (int j = 0; j < right.key_cnt; j++)
						right.children[j] = right.children[j + 1];
					update_index_node(right, root.children[i + 1]);
					// set root
					root.keys[i] = node.keys[node.key_cnt - 1];
					update_index_node(root, offset);
					underflow = false;
				}
			}
			if (underflow) {
				if (i < root.key_cnt - 1)
					merge_child(offset, i);
				else {
					merge_child(offset, i - 1);
					i--;
				}
			}
		}
		bool res = erase_nonunderflow(root.children[i], key);
		read_index_node(&root, offset);
		read_index_node(&node, root.children[i]);
		if (root.keys[i] != node.keys[node.key_cnt - 1]) {
			root.keys[i] = node.keys[node.key_cnt - 1];
			update_index_node(root, offset);
		}
		return res;
	}
}

bool bptree::erase(uint64_t key) {
	if (index_header.tree_height == 0)// B+ 树为空
		return false;
	bool res = erase_nonunderflow(index_header.root_offset, key);
	bpnode root;
	read_index_node(&root, index_header.root_offset);
	if (root.key_cnt == 0)
		init_index_header();
	while (root.key_cnt == 1 && root.node_type == 0x01) { // brunch
		free_index_node(index_header.root_offset);
		index_header.root_offset = root.children[0];
		index_header.tree_height--;
		read_index_node(&root, index_header.root_offset);
	}
	return res;
}

bool bptree::update(uint64_t key, std::string data) {
	if (index_header.tree_height == 0) // B+ 树为空
		return false;
	uint64_t offset = find_recursive(key, index_header.root_offset);
	if (offset == 0xffffffffffffffff)
		return false;
	else {
		if (!update_data(offset, data.c_str(), data.size())) {
			erase(key);
			insert(key, data);
		}
	}
	return true;
}

bptree::~bptree() {
	update_index_header();
	if (index_file.is_open())
		index_file.close();
}
