/*
* ���� B+ ���ļ����ݿ�
*/
#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// 254 �� B+ ��
#define ORDER 254
#define NODE_SIZE 0x1000

class bptree {
	std::string index_filename;	// �����ļ���
	std::string data_filename;	// �����ļ���
	struct {
		uint64_t root_offset;	// ���ڵ�ƫ����
		uint64_t tree_height;	// ����
		uint64_t node_cnt;		// �ڵ����
	} index_header;		// �����ļ�ͷ
	std::fstream index_file;	// �����ļ���
	std::fstream data_file;		// �����ļ���

public:
	class bpnode;
private:
	void		init_index_header();
public:
	bptree(const std::string& filename);
private:
	void		update_index_header();
	void		read_index_node(bpnode* node, uint64_t offset);
	std::string	read_data(uint64_t offset);
	void		update_index_node(bpnode node, uint64_t offset);
	uint64_t	alloc_index_node(bpnode node);
	uint64_t	alloc_data(const char* data, uint64_t size);
	bool		update_data(uint64_t offset, const char* data, uint64_t size);
	void		split_ith_child(uint64_t offset, int i);
	bool		insert_nonfull(uint64_t offset, uint64_t key, std::string data);
public:
	bool		insert(uint64_t key, std::string data);
private:
	uint64_t	find_recursive(uint64_t key, uint64_t offset);
public:
	std::string	find(uint64_t key);
private:
	uint64_t	find_index_node_recursive(uint64_t key, uint64_t offset);
public:
	std::vector<std::string> find_range(uint64_t left, uint64_t right);
private:
	void		free_index_node(uint64_t offset);
	void		merge_child(uint64_t offset, int i);
	int			find_idx(bpnode& node, uint64_t key);
	bool		erase_nonunderflow(uint64_t offset, uint64_t key);
public:
	bool		erase(uint64_t key);
	bool		update(uint64_t key, std::string data);
	~bptree();
};
