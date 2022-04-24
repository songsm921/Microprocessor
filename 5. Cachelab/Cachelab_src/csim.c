/*
Povis ID : songsm921
송수민
 */
#include "cachelab.h"
#include "stdio.h"
#include "stdlib.h"
#include <getopt.h>
#include "string.h"
#include "stdbool.h"
typedef struct {
	bool valid;
	int tag;
	int counter;
}Line;
typedef struct {
	Line *lines;
}Set;
typedef struct {
	Set *set;
	size_t num_set;
	size_t num_line;
}Cache;
Cache cache = {};
int init_set = 0;
int init_block = 0;
size_t hit = 0;
size_t miss = 0;
size_t eviction = 0;
void update(Set *set, size_t line_update);
void run(int address);
int main(int argc, char *argv[])
{
	FILE *file = 0;
	for (int option; (option = getopt(argc, argv, "s:E:b:t:")) != -1;) {
		switch(option){
			case 's':
				init_set = atoi(optarg);
				cache.num_set = 2 << init_set;
				break;
			case 'E':
				cache.num_line = atoi(optarg);
				break;
			case 'b':
				init_block = atoi(optarg);
				break;
			case 't':
				if (!(file = fopen(optarg, "r")))
					return 1;
				break;
			default:
				return 1;
		}
	}
	if (!init_set || !(cache.num_line) || !init_block || !file)
		return 1;
	cache.set = malloc(sizeof(Set)*cache.num_set);
	for (int i = 0; i < cache.num_set; i++) {
		cache.set[i].lines = calloc(sizeof(Line), cache.num_line);
	}
	char mode;
	int address;
	while (fscanf(file, " %c %x%*c%*d", &mode, &address) != EOF) {
		if (mode == 'I')
			continue;

		run(address);
		if (mode == 'M')
			run(address);
	}
	printSummary(hit, miss, eviction);
	fclose(file);
	for (size_t i = 0; i < cache.num_set; i++)
		free(cache.set[i].lines);
	free(cache.set);
	return 0;
}

void run(int address) {
	size_t set_index = (0x7fffffff >> (31 - init_set)) & (address >> init_block);
	int tag = 0xffffffff & (address >> (init_set + init_block));
	Set *set = &cache.set[set_index];
	for (size_t i = 0; i < cache.num_line; i++) {
		Line* line = &set->lines[i];
		if (!line->valid)
			continue;
		if (line->tag != tag)
			continue;
		++hit;
		update(set, i);
		return;
	}
	++miss;
	for (size_t i = 0; i < cache.num_line; i++) {
		Line* line = &set->lines[i];
		if (line->valid)
			continue;
		line->valid = true;
		line->tag = tag;
		update(set, i);
		return;
	}
	++eviction;
	for (size_t i = 0; i < cache.num_line; i++) {
		Line* line = &set->lines[i];
		if (line->counter)
			continue;
		line->valid = true;
		line->tag = tag;
		update(set, i);
		return;
	}
}


void update(Set *set, size_t line_update) {
	Line *line = &set->lines[line_update];
	for (size_t i = 0; i < cache.num_line; i++) {
		Line *iterator = &set->lines[i];
		if (!iterator->valid)
			continue;
		if (iterator->counter <= line->counter)
			continue;

		--iterator->counter;
	}
	line->counter = cache.num_line - 1;
}
