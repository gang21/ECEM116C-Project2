#include "cache.h"

cache::cache()
{
	for (int i=0; i<L1_CACHE_SETS; i++)
		L1[i].valid = false; 
	for (int i=0; i<L2_CACHE_SETS; i++)
		for (int j=0; j<L2_CACHE_WAYS; j++)
			L2[i][j].valid = false; 
	for (int i=0; i<VICTIM_SIZE; i++) {
		VICTIM[i].valid = false;
	}

	// Do the same for Victim Cache ...

	this->myStat.missL1 =0;
	this->myStat.missL2 =0;
	this->myStat.accL1 =0;
	this->myStat.accL2 =0;
	this->myStat.accVic =0;
	this->myStat.missVic =0;

	// Add stat for Victim cache ... 
	
}

void cache::swapL1Victim(cacheBlock l1, cacheBlock victim, int l1_index, int victim_index) {
	int old_lru = victim.lru_position;
	L1[l1_index] = victim;
	L1[l1_index].lru_position = -1;
	L1[l1_index].valid = 1;
	VICTIM[victim_index] = l1;
	VICTIM[victim_index].tag = l1.tag;
	VICTIM[victim_index].lru_position = 3;
	VICTIM[victim_index].address = l1.address;
	for (int j = 0; j < VICTIM_SIZE; j++) {
		if (j != victim_index && VICTIM[j].lru_position >= old_lru) {
			VICTIM[j].lru_position -= 1;
		}
	}
}



int cache::loadWord(int block_offset, int tag, int index, int adr, int* myMem) {
	myStat.accL1 += 1;
	//check L1 cache
	if (searchL1(tag, index, block_offset) == 1) {
		return L1[index].data;
	}	
	myStat.missL1 += 1;	//miss L1

	//check victim cache
	myStat.accVic += 1;
	for (int i=0; i < VICTIM_SIZE; i++) {
		if (VICTIM[i].valid && VICTIM[i].address == adr) {	//hit victim
			cacheBlock L1_data = L1[index];
			cacheBlock vic_data = VICTIM[i];
			int old_lru = vic_data.lru_position;
			swapL1Victim(L1_data, vic_data, index, i);
			return L1[index].data;
		}
	}
	myStat.missVic += 1; 	// miss victim
	
	//check L2 cache
	myStat.accL2 += 1;
	for (int i = 0; i < L2_CACHE_WAYS; i++) {
		if (L2[index][i].valid && L2[index][i].tag == tag) {	//L2 hit
			cacheBlock hit_block = L2[index][i];
			L2[index][i].valid = false;
			//update L1
			cacheBlock evictedL1 = updateL1(hit_block.tag, hit_block.index, hit_block.data, hit_block.blockOffset, hit_block.address);
			if (evictedL1.valid) {	//update victim
				cacheBlock evictedVictim = updateVictimLRU(evictedL1);
				if (evictedVictim.valid) {	//udpate L2
					updateL2(evictedVictim.tag, evictedVictim.index, evictedVictim.data, evictedVictim.blockOffset, evictedVictim.address);
				}
			}
			return L2[index][i].data;
		}
	}

	// int l2_ind = searchL2(tag, index, block_offset);
	// cout << "INDEX: " << l2_ind << endl;
	// if (l2_ind != -1) {		//hit L2
	// 	//bring value to L1
	// 	cacheBlock found_block = L2[index][l2_ind];
	// 	int ret_val = found_block.data;
	// 	L2[index][l2_ind].valid = false;
	// 	cacheBlock evictedl1 = updateL1(tag, index, found_block.data, block_offset, adr);	

	// 	// move evicted into victim
	// 	if (evictedl1.valid) {
	// 		cacheBlock evictedVictim = updateVictimLRU(evictedl1);

	// 		// move evicted into L2
	// 		if (evictedVictim.valid) {
	// 			updateL2(evictedVictim.tag, evictedVictim.index, evictedVictim.data, evictedVictim.blockOffset, evictedVictim.address);
	// 		}
	// 	}
	// 	return ret_val;
	// }
	myStat.missL2 += 1;

	//miss -> go to main memory (update all caches)
	int val = myMem[adr];
	//update L1
	cacheBlock oldVal = updateL1(tag, index, val, block_offset, adr);
	if (!oldVal.valid) {
		return val;
	}
	cacheBlock evictedVictim = updateVictimLRU(oldVal);
	if (evictedVictim.valid) {
		updateL2(evictedVictim.tag, evictedVictim.index, evictedVictim.data, evictedVictim.blockOffset, evictedVictim.address);
	}
	return val;
}

cacheBlock cache::updateVictimLRU(cacheBlock new_data) {
	//check for empty spot in victim cache
	bool isFull = true;
	int index = -1;
	for (int i = 0; i < VICTIM_SIZE; i++) {
		if (VICTIM[i].valid == 0) {
			isFull = false;
			index = i;
			break;
		}
	}
	if (!isFull) {
		//get old val to return
		cacheBlock oldval = VICTIM[index];
		oldval.valid = false;
		VICTIM[index] = new_data;
		VICTIM[index].tag = new_data.tag;
		VICTIM[index].valid = true;
		VICTIM[index].lru_position = 3;
		VICTIM[index].data = new_data.data;
		VICTIM[index].address = new_data.address;
		VICTIM[index].index = new_data.index;
		for (int i = 0; i < VICTIM_SIZE; i++) {
			if (i != index && VICTIM[index].valid == 1) {
				VICTIM[i].lru_position -= 1;
			}
		}
		return oldval;
	}
	// checking if it's a full victim cache
	else {
		cacheBlock oldval;
		for (int i = 0; i < VICTIM_SIZE; i++) {
			if (VICTIM[i].lru_position == 0) {
				oldval = VICTIM[i];
				VICTIM[i] = new_data;
				VICTIM[i].tag = new_data.tag;
				VICTIM[i].valid = true;
				VICTIM[i].lru_position = 3;
				VICTIM[i].data = new_data.data;
				VICTIM[i].address = new_data.address;
				VICTIM[i].index = new_data.index;
			}
			else {
				VICTIM[i].lru_position -= 1;
			}
		}
		return oldval;
	}

}

void cache::storeWord(int address, int block_offset, int tag, int index, int value) {
	//if L1 cache is empty (EDIT: don't do this because write no allocate)
	// if (L1[index].valid == 0) {
	// 	//add directly to L1 cache (done!)
	// 	cout << "adding to empty L1 slot" << endl;
	// 	cacheBlock block  = {};
	// 	block.blockOffset = block_offset;
	// 	block.data = value;
	// 	block.lru_position = -1;
	// 	block.tag = tag;
	// 	block.valid = 1;
	// 	block.address = address;
	// 	L1[index] = block;
	// 	cout << "--------------L1-------------" << endl;
	// 	cout << "index: " << index << "  |  tag: " << tag << "  |  valid: " << L1[index].valid << "  |  data: " << value << endl; 
	// 	cout << "--------------L1-------------" << endl;
	// 	return;
	// }

	//check L1 Cache first
	if (searchL1(tag, index, block_offset) == 1) { 	//hit L1
		cacheBlock oldVal = updateL1(tag, index, value, block_offset, address);
		updateVictimLRU(oldVal);
		return;
	}
	//check the victim cache
	for (int i = 0; i < VICTIM_SIZE; i++) {
		if (VICTIM[i].valid && VICTIM[i].tag == address) {		//hit victim (bring it up to L1)
			cacheBlock l1 = L1[index];
			cacheBlock victim = VICTIM[i];
			swapL1Victim(l1, victim, index, i);
			return;
		}
	}

	// check L2 cache
	for (int i = 0; i < L2_CACHE_WAYS; i++) {
		if (L2[index][i].tag == tag && L2[index][i].valid) {		//hit 
			//swap with L1
				cacheBlock hit_block = L2[index][i];
				L2[index][i].valid = false;
				cacheBlock evictedL1 = updateL1(hit_block.tag, hit_block.index, hit_block.data, hit_block.blockOffset, hit_block.address);
				if (evictedL1.valid) {	//update victim
					cacheBlock evictedVictim = updateVictimLRU(evictedL1);
					if (evictedVictim.valid) {
						updateL2(evictedVictim.tag, evictedVictim.index, evictedVictim.data, evictedVictim.blockOffset, evictedVictim.address);
					}

				}				

			// update L2
		}
	}
	


	// int L2_ind = searchL2(tag, index, block_offset);
	// cout << "L2 Index: " << L2_ind << endl;
	// if (L2_ind != -1) {
	// 	cout << "L2 SW HIT" << endl;
	// 	cacheBlock l2_block = L2[index][L2_ind];
	// 	cacheBlock evictedL1 = updateL1(tag, index, value, block_offset, address);
	// 	cacheBlock evictedVictim = updateVictimLRU(evictedL1);
	// 	updateL2(evictedVictim.tag, evictedVictim.index, evictedVictim.data, evictedVictim.blockOffset, evictedVictim.address);


	// }
			//if yes, swap L1 w/ L2
			// evict L1 into victim
			// evict victim into L2

	// not in any cache (update main memory (done!))
}
int cache::searchL1(int tag, int index, int block_offset) {
	if (L1[index].valid) {
		if (tag == L1[index].tag) {
			return 1;
			if (L1[index].blockOffset == block_offset) {
				return 1;
			}
		}
	}
	return 0;
}

int cache::searchL2(int tag, int index, int block_offset) {
	for (int i = 0; i < L2_CACHE_WAYS; i++) {
		if(L2[index][i].valid && L2[index][i].tag == tag) {
			return i;
		}
	}
	return -1;
}

cacheBlock cache::updateL1(int tag, int index, int value, int block_offset, int address) {
	cacheBlock oldVal = L1[index];
	L1[index].tag = tag;
	L1[index].blockOffset = block_offset;
	L1[index].data = value;
	L1[index].lru_position = -1;
	L1[index].valid = true;
	L1[index].address = address;
	L1[index].index = index;
	return oldVal;
}

void cache::updateL2(int tag, int index, int value, int block_offset, int address) {
	cacheBlock block = {};
	block.tag = tag;
	block.address = address;
	block.blockOffset = block_offset;
	block.data = value;
	block.lru_position = 7;
	block.valid = true;
	block.index = index;
	//check for empty spot in l2 cache
	int open_idx = -1;
	for (int i=0; i < L2_CACHE_WAYS; i++) {
		if(!L2[index][i].valid) {
			open_idx = i;
			break;
		}
	}
	//update all LRU positions
	int least_used_block_index = -1;
	for (int i = 0; i < L2_CACHE_WAYS; i++) {
		if (L2[index][i].valid) {
			L2[index][i].lru_position -= 1;
		}
		if (L2[index][i].lru_position == -1) {
			least_used_block_index = i;
		}
	}
	//add to L2 cache
	if(open_idx != -1) {	//open spot in cache
		L2[index][open_idx] = block;
	}
	else {
		L2[index][least_used_block_index] = block;
	}
}

void cache::controller(bool MemR, bool MemW, int* data, int adr, int* myMem)
{
	// getting block offset, tag, and index 
	bitset<32>address(adr);
	string addr = address.to_string();
	int block_offset, tag, index;
	string str_block_offset, str_tag, str_index = "";
	for (int i = 0; i < 32; i++) {
		if (i < 26)
			str_tag += addr[i];
		else if (i < 30)
			str_index += addr[i];
		else 
			str_block_offset += addr[i];

	}

	block_offset = stoi(str_block_offset, nullptr, 2);
	tag = stoi(str_tag, nullptr, 2);
	index = stoi(str_index, nullptr, 2);
	if (MemR) {
		loadWord(block_offset, tag, index, adr, myMem);
	}
	else {
		//write through (always write to main memory)
		myMem[adr] = *data;
		storeWord(adr, block_offset, tag, index, *data);
	}

}

float cache::getL1MissRate() {
	return ((float)myStat.missL1/(float)myStat.accL1);
}

float cache::getL2MissRate() {
	return ((float)myStat.missL2/(float)myStat.accL2); 
}

float cache::getVictimMissRate() {
	return ((float)myStat.missVic/(float)myStat.accVic); 
}

float cache::getAAT() {
	float hitTimeL1 = 1;
	float hitTimeL2 = 8;
	float hitTimeVictim = 1;
	float MissPenaltyMem = 100;

	return (hitTimeL1 + getL1MissRate()*(hitTimeVictim + getVictimMissRate()*(hitTimeL2 + getL2MissRate()* MissPenaltyMem)));
}