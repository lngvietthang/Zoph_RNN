#ifndef INPUT_FILE_PREP_H
#define INPUT_FILE_PREP_H

#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <stdlib.h>
#include <algorithm>
#include <queue>


struct comb_sent_info {

	std::vector<std::string> src_sent;
	std::vector<std::string> tgt_sent;

	std::vector<int> src_sent_int;
	std::vector<int> minus_two_source;
	std::vector<int> tgt_sent_int_i;
	std::vector<int> tgt_sent_int_o;
	int total_len;

	comb_sent_info(std::vector<std::string> &src_sent,std::vector<std::string> &tgt_sent) {
		this->src_sent = src_sent;
		this->tgt_sent = tgt_sent;
		total_len = tgt_sent.size() + src_sent.size();
	}
};

struct compare_nonLM {
    bool operator()(const struct comb_sent_info& first, const struct comb_sent_info& second) {
        return first.total_len < second.total_len;
    }
};

struct mapping_pair {
	std::string word;
	int count;
	mapping_pair(std::string word,int count) {
		this->word = word;
		this->count = count;
	}
};

struct mapping_pair_compare_functor {
  	bool operator() (mapping_pair &a,mapping_pair &b) const { return (a.count < b.count); }
};


//this will unk based on the source and target vocabulary
struct input_file_prep {

	std::ifstream source_input;
	std::ifstream target_input;
	std::ofstream final_output;

	std::unordered_map<std::string,int> src_mapping;
	std::unordered_map<std::string,int> tgt_mapping;

	std::unordered_map<int,std::string> tgt_reverse_mapping;
	std::unordered_map<int,std::string> src_reverse_mapping;

	std::unordered_map<std::string,int> src_counts;
	std::unordered_map<std::string,int> tgt_counts;

	int minibatch_mult = 10; //montreal uses 20
	std::vector<comb_sent_info> data; //can be used to sort by mult of minibatch


	bool prep_files_train_nonLM(int minibatch_size,int max_sent_cutoff,
		std::string source_file_name,std::string target_file_name,
		std::string output_file_name,int &source_vocab_size,int &target_vocab_size,
		bool shuffle,std::string model_output_file_name,int hiddenstate_size,int num_layers,bool unk_replace,int unk_align_range) 
	{


		int VISUAL_num_source_word_tokens =0;
		int VISUAL_total_source_vocab_size=0;
		int VISUAL_num_single_source_words=0;
		int VISUAL_num_segment_pairs=0;
		double VISUAL_avg_source_seg_len=0;
		int VISUAL_source_longest_sent=0;


		int VISUAL_num_target_word_tokens =0;
		int VISUAL_total_target_vocab_size=0;
		int VISUAL_num_single_target_words=0;
		VISUAL_num_segment_pairs=0;
		double VISUAL_avg_target_seg_len=0;
		int VISUAL_target_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;


		target_input.open(target_file_name.c_str());
		final_output.open(output_file_name.c_str());
		source_input.open(source_file_name.c_str());

		//first stage is load all data into RAM
		std::string src_str;
		std::string tgt_str; 
		std::string word;

		int source_len = 0;
		int target_len = 0;

		source_input.clear();
		target_input.clear();

		source_input.seekg(0, std::ios::beg);
		while(std::getline(source_input, src_str)) {
			source_len++;
		}

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		//do check to be sure the two files are the same length
		if(source_len!=target_len) {
			std::cerr << "ERROR: Input files are not the same length\n";
			return false;
			//exit (EXIT_FAILURE);
		}

		if(minibatch_size>source_len) {
			std::cerr << "ERROR: minibatch size cannot be greater than the file size\n";
			return false;
			//exit (EXIT_FAILURE);
		}


		//filter any long sentences and get ready to shuffle
		source_input.clear();
		target_input.clear();
		source_input.seekg(0, std::ios::beg);
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<source_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(source_input, src_str);
			std::getline(target_input, tgt_str);

			std::istringstream iss_src(src_str, std::istringstream::in);
			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_src >> word) {
				src_sentence.push_back(word);
			}
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}

			if( !(src_sentence.size()+1>=max_sent_cutoff-2 || tgt_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));
				VISUAL_avg_source_seg_len+=src_sentence.size();
				VISUAL_avg_target_seg_len+=tgt_sentence.size();
				VISUAL_num_source_word_tokens+=src_sentence.size();
				VISUAL_num_target_word_tokens+=tgt_sentence.size();

				if(VISUAL_source_longest_sent < src_sentence.size()) {
					VISUAL_source_longest_sent = src_sentence.size();
				}
				if(VISUAL_target_longest_sent < tgt_sentence.size()) {
					VISUAL_target_longest_sent = tgt_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}
		VISUAL_avg_source_seg_len = VISUAL_avg_source_seg_len/( (double)VISUAL_num_segment_pairs);
		VISUAL_avg_target_seg_len = VISUAL_avg_target_seg_len/( (double)VISUAL_num_segment_pairs);

		//shuffle the entire data
		if(shuffle) {
			std::random_shuffle(data.begin(),data.end());
		}


		//remove last sentences that do not fit in the minibatch
		if(data.size()%minibatch_size!=0) {
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: file size is zero, could be wrong input file or all lines are above max sent length\n";
			return false;
			exit (EXIT_FAILURE);
		}

		//sort the data based on minibatch
		compare_nonLM comp;
		int curr_index = 0;
		while(curr_index<data.size()) {
			if(curr_index+minibatch_size*minibatch_mult <= data.size()) {
				std::sort(data.begin()+curr_index,data.begin()+curr_index+minibatch_size*minibatch_mult,comp);
				curr_index+=minibatch_size*minibatch_mult;
			}
			else {
				std::sort(data.begin()+curr_index,data.end(),comp);
				break;
			}
		}


		//now get counts for mappings
		for(int i=0; i<data.size(); i++) {
			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(data[i].src_sent[j]!= "<UNK>") {
					if(src_counts.count(data[i].src_sent[j])==0) {
						src_counts[data[i].src_sent[j]] = 1;
					}
					else {
						src_counts[data[i].src_sent[j]]+=1;
					}
				}
			}

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(data[i].tgt_sent[j]!= "<UNK>") {
					if(tgt_counts.count(data[i].tgt_sent[j])==0) {
						tgt_counts[data[i].tgt_sent[j]] = 1;
					}
					else {
						tgt_counts[data[i].tgt_sent[j]]+=1;
					}
				}
			}
		}

		//now use heap to get the highest source and target mappings
		if(source_vocab_size==-1) {
			source_vocab_size = src_counts.size()+2;
		}
		if(target_vocab_size==-1) {
			if(!unk_replace) {
				target_vocab_size = tgt_counts.size()+3;
			}
			else {
				target_vocab_size = tgt_counts.size() + 3 + 1 + unk_align_range*2;
			}
		}

		VISUAL_total_source_vocab_size = src_counts.size();
		VISUAL_total_target_vocab_size = tgt_counts.size();

		if(!unk_replace) {
			source_vocab_size = std::min(source_vocab_size,(int)src_counts.size()+2);
			target_vocab_size = std::min(target_vocab_size,(int)tgt_counts.size()+3);
		}

		//output the model info to first line of output weights file
		std::ofstream output_model;
		output_model.open(model_output_file_name.c_str());
		output_model << num_layers << " " << hiddenstate_size << " " << target_vocab_size << " " << source_vocab_size << "\n";

		std::priority_queue<mapping_pair,std::vector<mapping_pair>, mapping_pair_compare_functor> src_map_heap;
		std::priority_queue<mapping_pair,std::vector<mapping_pair>, mapping_pair_compare_functor> tgt_map_heap;

		for ( auto it = src_counts.begin(); it != src_counts.end(); ++it ) {
			src_map_heap.push( mapping_pair(it->first,it->second) );
			if(it->second==1) {
				VISUAL_num_single_source_words++;
			}
		}

		for ( auto it = tgt_counts.begin(); it != tgt_counts.end(); ++it ) {
			tgt_map_heap.push( mapping_pair(it->first,it->second) );
			if(it->second==1) {
				VISUAL_num_single_target_words++;
			}
		}

		if(!unk_replace) {
			output_model << "==========================================================\n";
			src_mapping["<START>"] = 0;
			src_mapping["<UNK>"] = 1;
			output_model << 0 << " " << "<START>" << "\n";
			output_model << 1 << " " << "<UNK>" << "\n";

			for(int i=2; i<source_vocab_size; i++) {
				src_mapping[src_map_heap.top().word] = i;
				output_model << i << " " << src_map_heap.top().word << "\n";
				src_map_heap.pop();
			}
			// src_mapping["<UNK>"] = source_vocab_size-1;
			// output_model << source_vocab_size-1 << " " << "<UNK>" << "\n";
			output_model << "==========================================================\n";

			tgt_mapping["<START>"] = 0;
			tgt_mapping["<EOF>"] = 1;
			tgt_mapping["<UNK>"] = 2;
			output_model << 0 << " " << "<START>" << "\n";
			output_model << 1 << " " << "<EOF>" << "\n";
			output_model << 2 << " " << "<UNK>" << "\n";

			for(int i=3; i<target_vocab_size; i++) {
				tgt_mapping[tgt_map_heap.top().word] = i;
				output_model << i << " " << tgt_map_heap.top().word << "\n";
				tgt_map_heap.pop();
			}
			// tgt_mapping["<UNK>"] = target_vocab_size-1;
			// output_model << target_vocab_size-1 << " " << "<UNK>" << "\n";
			output_model << "==========================================================\n";
		}
		else {
			output_model << "==========================================================\n";
			src_mapping["<START>"] = 0;
			src_mapping["<UNK>"] = 1;
			output_model << 0 << " " << "<START>" << "\n";
			output_model << 1 << " " << "<UNK>" << "\n";

			for(int i=2; i<source_vocab_size; i++) {
				src_mapping[src_map_heap.top().word] = i;
				output_model << i << " " << src_map_heap.top().word << "\n";
				src_map_heap.pop();
			}
			// src_mapping["<UNK>"] = source_vocab_size-1;
			// output_model << source_vocab_size-1 << " " << "<UNK>" << "\n";
			output_model << "==========================================================\n";

			tgt_mapping["<START>"] = 0;
			tgt_mapping["<EOF>"] = 1;
			tgt_mapping["<UNK>NULL"] = 2;
			output_model << 0 << " " << "<START>" << "\n";
			output_model << 1 << " " << "<EOF>" << "\n";
			output_model << 2 << " " << "<UNK>NULL" << "\n";

			int curr_index = 3;
			for(int i= -unk_align_range; i < unk_align_range + 1; i++) {
				tgt_mapping["<UNK>"+std::to_string(i)] = curr_index;
				output_model << curr_index << " " << "<UNK>"+std::to_string(i) << "\n";
				curr_index++;
			}
			std::cout << "curr index " << curr_index << "\n";
			std::cout << "target vocab size " << target_vocab_size << "\n";
			for(int i=curr_index; i < target_vocab_size; i++) {
				tgt_mapping[tgt_map_heap.top().word] = i;
				output_model << i << " " << tgt_map_heap.top().word << "\n";
				tgt_map_heap.pop();
			}
			// tgt_mapping["<UNK>"] = target_vocab_size-1;
			// output_model << target_vocab_size-1 << " " << "<UNK>" << "\n";
			output_model << "==========================================================\n";
		}


		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> src_int;
			std::vector<int> tgt_int;
			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(src_mapping.count(data[i].src_sent[j])==0) {
					src_int.push_back(src_mapping["<UNK>"]);
				}
				else {
					src_int.push_back(src_mapping[data[i].src_sent[j]]);
				}	
			}
			std::reverse(src_int.begin(), src_int.end());
			data[i].src_sent.clear();
			data[i].src_sent_int = src_int;
			data[i].src_sent_int.insert(data[i].src_sent_int.begin(),0);
			while(data[i].minus_two_source.size()!=data[i].src_sent_int.size()) {
				data[i].minus_two_source.push_back(-2);
			}

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}
			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);

		}

		//now pad based on minibatch
		curr_index = 0;
		while(curr_index < data.size()) {
			int max_source_minibatch=0;
			int max_target_minibatch=0;

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				if(data[i].src_sent_int.size()>max_source_minibatch) {
					max_source_minibatch = data[i].src_sent_int.size();
				}
				if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
					max_target_minibatch = data[i].tgt_sent_int_i.size();
				}
			}

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {

				while(data[i].src_sent_int.size()<max_source_minibatch) {
					data[i].src_sent_int.insert(data[i].src_sent_int.begin(),-1);
					data[i].minus_two_source.insert(data[i].minus_two_source.begin(),-1);
				}

				while(data[i].tgt_sent_int_i.size()<max_target_minibatch) {
					data[i].tgt_sent_int_i.push_back(-1);
					data[i].tgt_sent_int_o.push_back(-1);
				}
			}
			curr_index+=minibatch_size;
		}

		//now output to the file
		for(int i=0; i<data.size(); i++) {

			for(int j=0; j<data[i].src_sent_int.size(); j++) {
				final_output << data[i].src_sent_int[j];
				if(j!=data[i].src_sent_int.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].minus_two_source.size(); j++) {
				final_output << data[i].minus_two_source[j];
				if(j!=data[i].minus_two_source.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}

		final_output.close();
		source_input.close();
		target_input.close();

		//print file stats:
		std::cout << "----------------------------source train file info-----------------------------\n";
		std::cout << "Number of source word tokens: " << VISUAL_num_source_word_tokens <<"\n";
		std::cout << "Source vocabulary size (before <unk>ing): " << VISUAL_total_source_vocab_size<<"\n";
		std::cout << "Number of source singleton word types: " << VISUAL_num_single_source_words<<"\n";
		std::cout << "Number of source segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average source segment length: " << VISUAL_avg_source_seg_len<< "\n";
		std::cout << "Longest source segment (after removing long sentences for training): " << VISUAL_source_longest_sent << "\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
		//print file stats:
		std::cout << "----------------------------target train file info-----------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Target vocabulary size (before <unk>ing): " << VISUAL_total_target_vocab_size<<"\n";
		std::cout << "Number of target singleton word types: " << VISUAL_num_single_target_words<<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average target segment length: " << VISUAL_avg_target_seg_len<< "\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_target_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff (source + target): " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
	
		return true;
	}


	bool prep_files_train_nonLM_ensemble(int minibatch_size,int max_sent_cutoff,
		std::string source_file_name,std::string target_file_name,
		std::string output_file_name,int &source_vocab_size,int &target_vocab_size,
		bool shuffle,std::string model_output_file_name,int hiddenstate_size,int num_layers,
		std::string ensemble_model_name)
	{

		int VISUAL_num_source_word_tokens =0;
		int VISUAL_total_source_vocab_size=0;
		int VISUAL_num_single_source_words=0;
		int VISUAL_num_segment_pairs=0;
		double VISUAL_avg_source_seg_len=0;
		int VISUAL_source_longest_sent=0;

		int VISUAL_num_target_word_tokens =0;
		int VISUAL_total_target_vocab_size=0;
		int VISUAL_num_single_target_words=0;
		VISUAL_num_segment_pairs=0;
		double VISUAL_avg_target_seg_len=0;
		int VISUAL_target_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;


		target_input.open(target_file_name.c_str());
		final_output.open(output_file_name.c_str());
		source_input.open(source_file_name.c_str());

		//first stage is load all data into RAM
		std::string src_str;
		std::string tgt_str; 
		std::string word;

		int source_len = 0;
		int target_len = 0;

		source_input.clear();
		target_input.clear();

		source_input.seekg(0, std::ios::beg);
		while(std::getline(source_input, src_str)) {
			source_len++;
		}

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		//do check to be sure the two files are the same length
		if(source_len!=target_len) {
			std::cerr << "ERROR: Input files are not the same length\n";
			return false;
			//exit (EXIT_FAILURE);
		}

		if(minibatch_size>source_len) {
			std::cerr << "ERROR: minibatch size cannot be greater than the file size\n";
			return false;
			//exit (EXIT_FAILURE);
		}


		//filter any long sentences and get ready to shuffle
		source_input.clear();
		target_input.clear();
		source_input.seekg(0, std::ios::beg);
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<source_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(source_input, src_str);
			std::getline(target_input, tgt_str);

			std::istringstream iss_src(src_str, std::istringstream::in);
			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_src >> word) {
				src_sentence.push_back(word);
			}
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}

			if( !(src_sentence.size()+1>=max_sent_cutoff-2 || tgt_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));
				VISUAL_avg_source_seg_len+=src_sentence.size();
				VISUAL_avg_target_seg_len+=tgt_sentence.size();
				VISUAL_num_source_word_tokens+=src_sentence.size();
				VISUAL_num_target_word_tokens+=tgt_sentence.size();

				if(VISUAL_source_longest_sent < src_sentence.size()) {
					VISUAL_source_longest_sent = src_sentence.size();
				}
				if(VISUAL_target_longest_sent < tgt_sentence.size()) {
					VISUAL_target_longest_sent = tgt_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}
		VISUAL_avg_source_seg_len = VISUAL_avg_source_seg_len/( (double)VISUAL_num_segment_pairs);
		VISUAL_avg_target_seg_len = VISUAL_avg_target_seg_len/( (double)VISUAL_num_segment_pairs);

		//shuffle the entire data
		if(shuffle) {
			std::random_shuffle(data.begin(),data.end());
		}


		//remove last sentences that do not fit in the minibatch
		if(data.size()%minibatch_size!=0) {
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: file size is zero, could be wrong input file or all lines are above max sent length\n";
			return false;
			exit (EXIT_FAILURE);
		}

		//sort the data based on minibatch
		compare_nonLM comp;
		int curr_index = 0;
		while(curr_index<data.size()) {
			if(curr_index+minibatch_size*minibatch_mult <= data.size()) {
				std::sort(data.begin()+curr_index,data.begin()+curr_index+minibatch_size*minibatch_mult,comp);
				curr_index+=minibatch_size*minibatch_mult;
			}
			else {
				std::sort(data.begin()+curr_index,data.end(),comp);
				break;
			}
		}


		//now get counts for mappings
		for(int i=0; i<data.size(); i++) {
			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(data[i].src_sent[j]!= "<UNK>") {
					if(src_counts.count(data[i].src_sent[j])==0) {
						src_counts[data[i].src_sent[j]] = 1;
					}
					else {
						src_counts[data[i].src_sent[j]]+=1;
					}
				}
			}

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(data[i].tgt_sent[j] != "<UNK>") {
					if(tgt_counts.count(data[i].tgt_sent[j])==0) {
						tgt_counts[data[i].tgt_sent[j]] = 1;
					}
					else {
						tgt_counts[data[i].tgt_sent[j]]+=1;
					}
				}
			}
		}

		VISUAL_total_source_vocab_size = src_counts.size();
		VISUAL_total_target_vocab_size = tgt_counts.size();



		for ( auto it = src_counts.begin(); it != src_counts.end(); ++it ) {
			if(it->second==1) {
				VISUAL_num_single_source_words++;
			}
		}

		for ( auto it = tgt_counts.begin(); it != tgt_counts.end(); ++it ) {
			if(it->second==1) {
				VISUAL_num_single_target_words++;
			}
		}


		//now load in the integer mappings from the other file for ensemble training
		std::ifstream ensemble_file;
		ensemble_file.open(ensemble_model_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;

		std::getline(ensemble_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=4) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		target_vocab_size = std::stoi(file_input_vec[2]);
		source_vocab_size = std::stoi(file_input_vec[3]);


		std::ofstream output_model;
		output_model.open(model_output_file_name.c_str());
		output_model << num_layers << " " << hiddenstate_size << " " << target_vocab_size << " " << source_vocab_size << "\n";

		output_model << "==========================================================\n";

		//now get the mappings
		std::getline(ensemble_file, str); //get this line, since all equals
		while(std::getline(ensemble_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with source mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			src_mapping[word] = tmp_index;
			output_model << tmp_index << " " << word << "\n";
		}

		output_model << "==========================================================\n";
		while(std::getline(ensemble_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_mapping[word] = tmp_index;
			output_model << tmp_index << " " << word << "\n";
		}

		output_model << "==========================================================\n";
		ensemble_file.close();


		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> src_int;
			std::vector<int> tgt_int;
			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(src_mapping.count(data[i].src_sent[j])==0) {
					src_int.push_back(src_mapping["<UNK>"]);
				}
				else {
					src_int.push_back(src_mapping[data[i].src_sent[j]]);
				}	
			}
			std::reverse(src_int.begin(), src_int.end());
			data[i].src_sent.clear();
			data[i].src_sent_int = src_int;
			data[i].src_sent_int.insert(data[i].src_sent_int.begin(),0);
			while(data[i].minus_two_source.size()!=data[i].src_sent_int.size()) {
				data[i].minus_two_source.push_back(-2);
			}

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}
			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);

		}

		//now pad based on minibatch
		curr_index = 0;
		while(curr_index < data.size()) {
			int max_source_minibatch=0;
			int max_target_minibatch=0;

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				if(data[i].src_sent_int.size()>max_source_minibatch) {
					max_source_minibatch = data[i].src_sent_int.size();
				}
				if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
					max_target_minibatch = data[i].tgt_sent_int_i.size();
				}
			}

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {

				while(data[i].src_sent_int.size()<max_source_minibatch) {
					data[i].src_sent_int.insert(data[i].src_sent_int.begin(),-1);
					data[i].minus_two_source.insert(data[i].minus_two_source.begin(),-1);
				}
				while(data[i].tgt_sent_int_i.size()<max_target_minibatch) {
					data[i].tgt_sent_int_i.push_back(-1);
					data[i].tgt_sent_int_o.push_back(-1);
				}
			}
			curr_index+=minibatch_size;
		}

		//now output to the file
		for(int i=0; i<data.size(); i++) {

			for(int j=0; j<data[i].src_sent_int.size(); j++) {
				final_output << data[i].src_sent_int[j];
				if(j!=data[i].src_sent_int.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].minus_two_source.size(); j++) {
				final_output << data[i].minus_two_source[j];
				if(j!=data[i].minus_two_source.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}

		final_output.close();
		source_input.close();
		target_input.close();

		//print file stats:
		std::cout << "----------------------------source train file info-----------------------------\n";
		std::cout << "Number of source word tokens: " << VISUAL_num_source_word_tokens <<"\n";
		std::cout << "Source vocabulary size (before <unk>ing): " << VISUAL_total_source_vocab_size<<"\n";
		std::cout << "Number of source singleton word types: " << VISUAL_num_single_source_words<<"\n";
		std::cout << "Number of source segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average source segment length: " << VISUAL_avg_source_seg_len<< "\n";
		std::cout << "Longest source segment (after removing long sentences for training): " << VISUAL_source_longest_sent << "\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
		//print file stats:
		std::cout << "----------------------------target train file info-----------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Target vocabulary size (before <unk>ing): " << VISUAL_total_target_vocab_size<<"\n";
		std::cout << "Number of target singleton word types: " << VISUAL_num_single_target_words<<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average target segment length: " << VISUAL_avg_target_seg_len<< "\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_target_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff (source + target): " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
	
		return true;
	}



	//carve up the minibatch and carry over the hiddenstate and cell state
	void prep_files_train_LM_carve(int minibatch_size,
		std::string target_file_name,
		std::string output_file_name,int &target_vocab_size,
		std::string model_output_file_name,int hiddenstate_size,int num_layers,
		int backprop_len) 
	{

		int VISUAL_num_target_word_tokens =0;
		int VISUAL_total_target_vocab_size=0;
		int VISUAL_num_single_target_words=0;
		int VISUAL_num_segment_pairs=0;
		double VISUAL_avg_target_seg_len=0;
		int VISUAL_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		std::vector<std::string> words; //make the data one long sentences, then carve it up
		target_input.open(target_file_name.c_str());
		final_output.open(output_file_name.c_str());

		int target_len = 0;

		std::string tgt_str; 
		std::string word;
		target_input.clear();
		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			words.push_back(tgt_str);
			target_len++;
		}

		int total_words = words.size();
		int words_per_minibatch = total_words/minibatch_size;
		if(words_per_minibatch%backprop_len!=0) {
			words_per_minibatch = words_per_minibatch - words_per_minibatch%backprop_len;
		}
		int num_to_remove = words.size() - words_per_minibatch*minibatch_size;
		
		for(int i=0; i<num_to_remove; i++) {
			words.pop_back();
		}
		total_words = words.size();

		std::vector<std::vector<std::string>> minibatched_data; //size of minibatch by words per minibatch
		std::vector<std::vector<int>> minibatched_data_int;

		for(int i=0; i<minibatch_size; i++) {
			std::vector<std::string> temp;
			std::vector<int> temp1;
			minibatched_data.push_back(temp);
			minibatched_data_int.push_back(temp1);
		}

		for(int i=0; i<words.size(); i++) {
			int curr_index = i/words_per_minibatch;
			minibatched_data[curr_index].push_back(words[i]);
		}

		//now get counts for mappings
		for(int i=0; i<words.size(); i++) {
			if(words[i] != "<UNK>") {
				if(tgt_counts.count(words[i])==0) {
					tgt_counts[words[i]] = 1;
				}
				else {
					tgt_counts[words[i]] += 1;
				}
			}
		}

		words.clear(); //for less memory

		//now use heap to get the highest source and target mappings
		if(target_vocab_size==-1) {
			target_vocab_size = tgt_counts.size()+3;
		}

		VISUAL_total_target_vocab_size = tgt_counts.size();

		target_vocab_size = std::min(target_vocab_size,(int)tgt_counts.size()+3);

		//output the model info to first line of output weights file
		std::ofstream output_model;
		output_model.open(model_output_file_name.c_str());
		output_model << num_layers << " " << hiddenstate_size << " " << target_vocab_size << "\n";

		std::priority_queue<mapping_pair,std::vector<mapping_pair>, mapping_pair_compare_functor> tgt_map_heap;

		for ( auto it = tgt_counts.begin(); it != tgt_counts.end(); ++it ) {
			tgt_map_heap.push( mapping_pair(it->first,it->second) );
			if(it->second==1) {
				VISUAL_num_single_target_words++;
			}
		}

		output_model << "==========================================================\n";
		tgt_mapping["<START>"] = 0;
		tgt_mapping["<UNK>"] = 1;
		output_model << 0 << " " << "<START>" << "\n";
		output_model << 1 << " " << "<UNK>" << "\n";

		for(int i=1; i<target_vocab_size; i++) {
			tgt_mapping[tgt_map_heap.top().word] = i;
			output_model << i << " " << tgt_map_heap.top().word << "\n";
			tgt_map_heap.pop();
		}
		output_model << "==========================================================\n";


		//now integerize
		for(int i=0; i<minibatched_data.size(); i++) {
			for(int j=0; j<minibatched_data[i].size(); j++) {
				if(tgt_mapping.count(minibatched_data[i][j])==0) {
					minibatched_data_int[i].push_back(tgt_mapping["<UNK>"]);
				}
				else {
					minibatched_data_int[i].push_back(tgt_mapping[minibatched_data[i][j]]);
				}	
			}
		}


		//now output to the file
		int minibatch_index = 0;
		int curr_word_index = 0;
		for(int i=0; i< total_words/backprop_len; i++) {
			final_output << "\n";
			final_output << "\n";

			for(int j=0; j < backprop_len; j++) {
				if(j==0) {
					final_output << tgt_mapping["<START>"]; //always put a start symbol
				}
				else {
					final_output << minibatched_data_int[minibatch_index][j+curr_word_index-1];
				}

				if(j!=backprop_len-1) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j< backprop_len; j++) {
				final_output << minibatched_data_int[minibatch_index][j+curr_word_index];
				if(j!= backprop_len-1) {
					final_output << " ";
				}
			}
			final_output << "\n";

			minibatch_index++;
			if(minibatch_index>=minibatch_size) {
				minibatch_index = 0;
				curr_word_index+=backprop_len;
			}
		}

		final_output.close();
		target_input.close();
		output_model.close();
	}



	bool prep_files_train_LM(int minibatch_size,int max_sent_cutoff,
		std::string target_file_name,
		std::string output_file_name,int &target_vocab_size,
		bool shuffle,std::string model_output_file_name,int hiddenstate_size,int num_layers) 
	{

		int VISUAL_num_target_word_tokens =0;
		int VISUAL_total_target_vocab_size=0;
		int VISUAL_num_single_target_words=0;
		int VISUAL_num_segment_pairs=0;
		double VISUAL_avg_target_seg_len=0;
		int VISUAL_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		target_input.open(target_file_name.c_str());
		final_output.open(output_file_name.c_str());
		//first stage is load all data into RAM
		std::string tgt_str; 
		std::string word;

		int target_len = 0;

		target_input.clear();

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		if(minibatch_size>target_len) {
			std::cerr << "ERROR: minibatch size cannot be greater than the file size\n";
			return false;
			//exit (EXIT_FAILURE);
		}


		double VISUAL_tmp_running_seg_len=0;

		//filter any long sentences and get ready to shuffle
		target_input.clear();
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<target_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(target_input, tgt_str);

			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}
			if( !(src_sentence.size()+1>=max_sent_cutoff-2 || tgt_sentence.size() + 1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));
				VISUAL_tmp_running_seg_len+=tgt_sentence.size();
				VISUAL_num_target_word_tokens+=tgt_sentence.size();
				if(tgt_sentence.size() > VISUAL_longest_sent) {
					VISUAL_longest_sent = tgt_sentence.size() ;
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}

		VISUAL_avg_target_seg_len = VISUAL_tmp_running_seg_len/VISUAL_num_segment_pairs;


		//shuffle the entire data
		if(shuffle) {
			std::random_shuffle(data.begin(),data.end());
		}

		//remove last sentences that do not fit in the minibatch
		if(data.size()%minibatch_size!=0) {
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: your dataset if of length zero\n";
			return false;
			//exit (EXIT_FAILURE);
		}

		//sort the data based on minibatch
		compare_nonLM comp;
		int curr_index = 0;
		while(curr_index<data.size()) {
			if(curr_index+minibatch_size*minibatch_mult <= data.size()) {
				std::sort(data.begin()+curr_index,data.begin()+curr_index+minibatch_size*minibatch_mult,comp);
				curr_index+=minibatch_size*minibatch_mult;
			}
			else {
				std::sort(data.begin()+curr_index,data.end(),comp);
				break;
			}
		}


		//now get counts for mappings
		for(int i=0; i<data.size(); i++) {
			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(data[i].tgt_sent[j] != "<UNK>") {
					if(tgt_counts.count(data[i].tgt_sent[j])==0) {

						tgt_counts[data[i].tgt_sent[j]] = 1;
					}
					else {
						tgt_counts[data[i].tgt_sent[j]]+=1;
					}
				}
			}
		}


		//now use heap to get the highest source and target mappings
		if(target_vocab_size==-1) {
			target_vocab_size = tgt_counts.size()+3;
		}

		VISUAL_total_target_vocab_size = tgt_counts.size();

		target_vocab_size = std::min(target_vocab_size,(int)tgt_counts.size()+3);

		//output the model info to first line of output weights file
		std::ofstream output_model;
		output_model.open(model_output_file_name.c_str());
		output_model << num_layers << " " << hiddenstate_size << " " << target_vocab_size << "\n";

		std::priority_queue<mapping_pair,std::vector<mapping_pair>, mapping_pair_compare_functor> tgt_map_heap;

		for ( auto it = tgt_counts.begin(); it != tgt_counts.end(); ++it ) {
			tgt_map_heap.push( mapping_pair(it->first,it->second) );
			if(it->second==1) {
				VISUAL_num_single_target_words++;
			}
		}

		output_model << "==========================================================\n";
		tgt_mapping["<START>"] = 0;
		tgt_mapping["<EOF>"] = 1;
		tgt_mapping["<UNK>"] = 2;
		output_model << 0 << " " << "<START>" << "\n";
		output_model << 1 << " " << "<EOF>" << "\n";
		output_model << 2 << " " << "<UNK>" << "\n";

		for(int i=3; i<target_vocab_size; i++) {
			tgt_mapping[tgt_map_heap.top().word] = i;
			output_model << i << " " << tgt_map_heap.top().word << "\n";
	
			tgt_map_heap.pop();
		}
		// tgt_mapping["<UNK>"] = target_vocab_size-1;
		// output_model << target_vocab_size-1 << " " << "<UNK>" << "\n";
		output_model << "==========================================================\n";


		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> tgt_int;

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}
			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);
		}

		//now pad based on minibatch
		curr_index = 0;
		while(curr_index < data.size()) {
			int max_target_minibatch=0;

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
					max_target_minibatch = data[i].tgt_sent_int_i.size();
				}
			}

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				while(data[i].tgt_sent_int_i.size()<=max_target_minibatch) {
					data[i].tgt_sent_int_i.push_back(-1);
					data[i].tgt_sent_int_o.push_back(-1);
				}
			}
			curr_index+=minibatch_size;
		}

		//now output to the file
		for(int i=0; i<data.size(); i++) {
			final_output << "\n";
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}

		final_output.close();
		target_input.close();

		//print file stats:
		std::cout << "----------------------------target train file info-------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Target vocabulary size (before <unk>ing): " << VISUAL_total_target_vocab_size<<"\n";
		std::cout << "Number of target singleton word types: " << VISUAL_num_single_target_words<<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average target segment length: " << VISUAL_avg_target_seg_len<< "\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff: " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
		
		return true;
	}

	bool prep_files_train_LM_ensemble(int minibatch_size,int max_sent_cutoff,
		std::string target_file_name,
		std::string output_file_name,int &target_vocab_size,
		bool shuffle,std::string model_output_file_name,int hiddenstate_size,int num_layers,std::string ensemble_model_name) 
	{

		int VISUAL_num_target_word_tokens =0;
		int VISUAL_total_target_vocab_size=0;
		int VISUAL_num_single_target_words=0;
		int VISUAL_num_segment_pairs=0;
		double VISUAL_avg_target_seg_len=0;
		int VISUAL_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		target_input.open(target_file_name.c_str());
		final_output.open(output_file_name.c_str());
		//first stage is load all data into RAM
		std::string tgt_str; 
		std::string word;

		int target_len = 0;

		target_input.clear();

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		if(minibatch_size>target_len) {
			std::cerr << "ERROR: minibatch size cannot be greater than the file size\n";
			return false;
			//exit (EXIT_FAILURE);
		}


		double VISUAL_tmp_running_seg_len=0;

		//filter any long sentences and get ready to shuffle
		target_input.clear();
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<target_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(target_input, tgt_str);

			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}
			if( !(src_sentence.size()+1>=max_sent_cutoff-2 || tgt_sentence.size() + 1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));
				VISUAL_tmp_running_seg_len+=tgt_sentence.size();
				VISUAL_num_target_word_tokens+=tgt_sentence.size();
				if(tgt_sentence.size() > VISUAL_longest_sent) {
					VISUAL_longest_sent = tgt_sentence.size() ;
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}

		VISUAL_avg_target_seg_len = VISUAL_tmp_running_seg_len/VISUAL_num_segment_pairs;


		//shuffle the entire data
		if(shuffle) {
			std::random_shuffle(data.begin(),data.end());
		}

		//remove last sentences that do not fit in the minibatch
		if(data.size()%minibatch_size!=0) {
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: your dataset if of length zero\n";
			return false;
			//exit (EXIT_FAILURE);
		}

		//sort the data based on minibatch
		compare_nonLM comp;
		int curr_index = 0;
		while(curr_index<data.size()) {
			if(curr_index+minibatch_size*minibatch_mult <= data.size()) {
				std::sort(data.begin()+curr_index,data.begin()+curr_index+minibatch_size*minibatch_mult,comp);
				curr_index+=minibatch_size*minibatch_mult;
			}
			else {
				std::sort(data.begin()+curr_index,data.end(),comp);
				break;
			}
		}


		//now get counts for mappings
		for(int i=0; i<data.size(); i++) {
			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(data[i].tgt_sent[j] != "<UNK>") {
					if(tgt_counts.count(data[i].tgt_sent[j])==0) {
						tgt_counts[data[i].tgt_sent[j]] = 1;
					}
					else {
						tgt_counts[data[i].tgt_sent[j]]+=1;
					}
				}
			}
		}


		VISUAL_total_target_vocab_size = tgt_counts.size();



		//now load in the integer mappings from the other file for ensemble training
		std::ifstream ensemble_file;
		ensemble_file.open(ensemble_model_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;

		std::getline(ensemble_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=3) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		target_vocab_size = std::stoi(file_input_vec[2]);

		std::ofstream output_model;
		output_model.open(model_output_file_name.c_str());
		output_model << num_layers << " " << hiddenstate_size << " " << target_vocab_size << "\n";

		output_model << "==========================================================\n";
		std::getline(ensemble_file, str);
		while(std::getline(ensemble_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_mapping[word] = tmp_index;
			output_model << tmp_index << " " << word << "\n";
		}

		output_model << "==========================================================\n";
		ensemble_file.close();


		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> tgt_int;

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}
			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);
		}

		//now pad based on minibatch
		curr_index = 0;
		while(curr_index < data.size()) {
			int max_target_minibatch=0;

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
					max_target_minibatch = data[i].tgt_sent_int_i.size();
				}
			}

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				while(data[i].tgt_sent_int_i.size()<=max_target_minibatch) {
					data[i].tgt_sent_int_i.push_back(-1);
					data[i].tgt_sent_int_o.push_back(-1);
				}
			}
			curr_index+=minibatch_size;
		}

		//now output to the file
		for(int i=0; i<data.size(); i++) {
			final_output << "\n";
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}

		final_output.close();
		target_input.close();

		//print file stats:
		std::cout << "----------------------------target train file info-------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Target vocabulary size (before <unk>ing): " << VISUAL_total_target_vocab_size<<"\n";
		std::cout << "Number of target singleton word types: " << VISUAL_num_single_target_words<<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Average target segment length: " << VISUAL_avg_target_seg_len<< "\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff: " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
		
		return true;
	}


	//for reading file from user input, then mapping to tmp/, such as dev sets, decoding input,stoic input, etc..
	void integerize_file_nonLM(std::string output_weights_name,std::string source_file_name,std::string target_file_name,std::string tmp_output_name,
		int max_sent_cutoff,int minibatch_size,int &hiddenstate_size,int &source_vocab_size,int &target_vocab_size,int &num_layers) 
	{

		int VISUAL_num_source_word_tokens =0;
		int VISUAL_num_segment_pairs=0;
		int VISUAL_source_longest_sent=0;


		int VISUAL_num_target_word_tokens =0;
		VISUAL_num_segment_pairs=0;
		int VISUAL_target_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;


		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;
		std::string word;

		std::getline(weights_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=4) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		num_layers = std::stoi(file_input_vec[0]);
		hiddenstate_size = std::stoi(file_input_vec[1]);
		target_vocab_size = std::stoi(file_input_vec[2]);
		source_vocab_size = std::stoi(file_input_vec[3]);

		//now get the mappings
		std::getline(weights_file, str); //get this line, since all equals
		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with source mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			src_mapping[word] = tmp_index;
		}

		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_mapping[word] = tmp_index;
		}

		//now that we have the mappings, integerize the file
		std::ofstream final_output;
		final_output.open(tmp_output_name.c_str());
		std::ifstream source_input;
		source_input.open(source_file_name.c_str());
		std::ifstream target_input;
		target_input.open(target_file_name.c_str());


		//first get the number of lines the the files and check to be sure they are the same
		int source_len = 0;
		int target_len = 0;
		std::string src_str;
		std::string tgt_str;

		source_input.clear();
		target_input.clear();

		source_input.seekg(0, std::ios::beg);
		while(std::getline(source_input, src_str)) {
			source_len++;
		}

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		//do check to be sure the two files are the same length
		if(source_len!=target_len) {
			std::cerr << "ERROR: Input files are not the same length\n";
			exit (EXIT_FAILURE);
		}


		source_input.clear();
		target_input.clear();
		source_input.seekg(0, std::ios::beg);
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<source_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(source_input, src_str);
			std::getline(target_input, tgt_str);

			std::istringstream iss_src(src_str, std::istringstream::in);
			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_src >> word) {
				src_sentence.push_back(word);
			}
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}

			if( !(src_sentence.size()+1>=max_sent_cutoff-2 || tgt_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));

				VISUAL_num_source_word_tokens+=src_sentence.size();
				VISUAL_num_target_word_tokens+=tgt_sentence.size();

				if(VISUAL_source_longest_sent < src_sentence.size()) {
					VISUAL_source_longest_sent = src_sentence.size();
				}
				if(VISUAL_target_longest_sent < tgt_sentence.size()) {
					VISUAL_target_longest_sent = tgt_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}	


		if(data.size()%minibatch_size!=0) {
			std::random_shuffle(data.begin(),data.end());
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: file size is zero, could be wrong input file or all lines are above max sent length\n";
			exit (EXIT_FAILURE);
		}

		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> src_int;
			std::vector<int> tgt_int;
			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(src_mapping.count(data[i].src_sent[j])==0) {
					//src_int.push_back(source_vocab_size-1);
					src_int.push_back(src_mapping["<UNK>"]);
				}
				else {
					src_int.push_back(src_mapping[data[i].src_sent[j]]);
				}	
			}
			std::reverse(src_int.begin(), src_int.end());
			data[i].src_sent.clear();
			data[i].src_sent_int = src_int;
			data[i].src_sent_int.insert(data[i].src_sent_int.begin(),0);
			while(data[i].minus_two_source.size()!=data[i].src_sent_int.size()) {
				data[i].minus_two_source.push_back(-2);
			}

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					//tgt_int.push_back(target_vocab_size-1);
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}
			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);

		}

		//now pad
		int curr_index = 0;
		while(curr_index < data.size()) {
			int max_source_minibatch=0;
			int max_target_minibatch=0;

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
				if(data[i].src_sent_int.size()>max_source_minibatch) {
					max_source_minibatch = data[i].src_sent_int.size();
				}
				if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
					max_target_minibatch = data[i].tgt_sent_int_i.size();
				}
			}

			for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {

				while(data[i].src_sent_int.size()<=max_source_minibatch) {
					data[i].src_sent_int.insert(data[i].src_sent_int.begin(),-1);
					data[i].minus_two_source.insert(data[i].minus_two_source.begin(),-1);
				}

				while(data[i].tgt_sent_int_i.size()<=max_target_minibatch) {
					data[i].tgt_sent_int_i.push_back(-1);
					data[i].tgt_sent_int_o.push_back(-1);
				}
			}
			curr_index+=minibatch_size;
		}


		for(int i=0; i<data.size(); i++) {

			for(int j=0; j<data[i].src_sent_int.size(); j++) {
				final_output << data[i].src_sent_int[j];
				if(j!=data[i].src_sent_int.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].minus_two_source.size(); j++) {
				final_output << data[i].minus_two_source[j];
				if(j!=data[i].minus_two_source.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}


		weights_file.close();
		final_output.close();
		source_input.close();
		target_input.close();


		//print file stats:
		std::cout << "----------------------------source dev file info-----------------------------\n";
		std::cout << "Number of source word tokens: " << VISUAL_num_source_word_tokens <<"\n";
		std::cout << "Number of source segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Longest source segment (after removing long sentences for training): " << VISUAL_source_longest_sent << "\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
		//print file stats:
		std::cout << "----------------------------target dev file info-----------------------------\n\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_target_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff (source + target): " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";
	}


	void integerize_file_LM_carve(std::string output_weights_name,std::string target_file_name,std::string tmp_output_name,
		int max_sent_cutoff,int minibatch_size,bool dev,int &hiddenstate_size,int &target_vocab_size,int &num_layers) 
	{


		int VISUAL_num_target_word_tokens =0;
		int VISUAL_num_segment_pairs=0;
		int VISUAL_target_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;
		std::string word;

		std::getline(weights_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=3) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		num_layers = std::stoi(file_input_vec[0]);
		hiddenstate_size = std::stoi(file_input_vec[1]);
		target_vocab_size = std::stoi(file_input_vec[2]);

		//now get the target mappings
		std::getline(weights_file, str); //get this line, since all equals
		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_mapping[word] = tmp_index;
		}

		//now that we have the mappings, integerize the file
		std::ofstream final_output;
		final_output.open(tmp_output_name.c_str());
		std::ifstream target_input;
		target_input.open(target_file_name.c_str());


		//first get the number of lines the the files and check to be sure they are the same
		int target_len = 0;
		std::string tgt_str;

		target_input.clear();

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		target_input.clear();
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<target_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(target_input, tgt_str);

			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}

			if( !(tgt_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));

				VISUAL_num_target_word_tokens+=tgt_sentence.size();

				if(VISUAL_target_longest_sent < tgt_sentence.size()) {
					VISUAL_target_longest_sent = tgt_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}


		if(data.size()%minibatch_size!=0) {
			std::random_shuffle(data.begin(),data.end());
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: file size is zero, could be wrong input file or all lines are above max sent length\n";
			exit (EXIT_FAILURE);
		}

		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> tgt_int;

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					//tgt_int.push_back(target_vocab_size-1);
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}

			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);

		}

		//now pad based on minibatch
		if(dev) {
			int curr_index = 0;
			while(curr_index < data.size()) {
				int max_target_minibatch=0;

				for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
					if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
						max_target_minibatch = data[i].tgt_sent_int_i.size();
					}
				}

				for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
					while(data[i].tgt_sent_int_i.size()<=max_target_minibatch) {
						data[i].tgt_sent_int_i.push_back(-1);
						data[i].tgt_sent_int_o.push_back(-1);
					}
				}
				curr_index+=minibatch_size;
			}
		}

		for(int i=0; i<data.size(); i++) {

			final_output << "\n";
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}


		weights_file.close();
		final_output.close();
		target_input.close();

		std::cout << "----------------------------target dev file info-----------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_target_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff: " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n";
	}



	void integerize_file_LM(std::string output_weights_name,std::string target_file_name,std::string tmp_output_name,
		int max_sent_cutoff,int minibatch_size,bool dev,int &hiddenstate_size,int &target_vocab_size,int &num_layers) 
	{


		int VISUAL_num_target_word_tokens =0;
		int VISUAL_num_segment_pairs=0;
		int VISUAL_target_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;
		std::string word;

		std::getline(weights_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=3) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		num_layers = std::stoi(file_input_vec[0]);
		hiddenstate_size = std::stoi(file_input_vec[1]);
		target_vocab_size = std::stoi(file_input_vec[2]);

		//now get the target mappings
		std::getline(weights_file, str); //get this line, since all equals
		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_mapping[word] = tmp_index;
		}

		//now that we have the mappings, integerize the file
		std::ofstream final_output;
		final_output.open(tmp_output_name.c_str());
		std::ifstream target_input;
		target_input.open(target_file_name.c_str());


		//first get the number of lines the the files and check to be sure they are the same
		int target_len = 0;
		std::string tgt_str;

		target_input.clear();

		target_input.seekg(0, std::ios::beg);
		while(std::getline(target_input, tgt_str)) {
			target_len++;
		}

		VISUAL_num_segment_pairs = target_len;

		target_input.clear();
		target_input.seekg(0, std::ios::beg);
		for(int i=0; i<target_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(target_input, tgt_str);

			std::istringstream iss_tgt(tgt_str, std::istringstream::in);
			while(iss_tgt >> word) {
				tgt_sentence.push_back(word);
			}

			if( !(tgt_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));

				VISUAL_num_target_word_tokens+=tgt_sentence.size();

				if(VISUAL_target_longest_sent < tgt_sentence.size()) {
					VISUAL_target_longest_sent = tgt_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}


		if(data.size()%minibatch_size!=0) {
			std::random_shuffle(data.begin(),data.end());
			int num_to_remove = data.size()%minibatch_size;
			for(int i=0; i<num_to_remove; i++) {
				data.pop_back();
			}
		}

		if(data.size()==0) {
			std::cout << "ERROR: file size is zero, could be wrong input file or all lines are above max sent length\n";
			exit (EXIT_FAILURE);
		}

		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> tgt_int;

			for(int j=0; j<data[i].tgt_sent.size(); j++) {
				if(tgt_mapping.count(data[i].tgt_sent[j])==0) {
					//tgt_int.push_back(target_vocab_size-1);
					tgt_int.push_back(tgt_mapping["<UNK>"]);
				}
				else {
					tgt_int.push_back(tgt_mapping[data[i].tgt_sent[j]]);
				}	
			}

			data[i].tgt_sent.clear();
			data[i].tgt_sent_int_i = tgt_int;
			data[i].tgt_sent_int_o = tgt_int;
			data[i].tgt_sent_int_i.insert(data[i].tgt_sent_int_i.begin(),0);
			data[i].tgt_sent_int_o.push_back(1);

		}

		//now pad based on minibatch
		if(dev) {
			int curr_index = 0;
			while(curr_index < data.size()) {
				int max_target_minibatch=0;

				for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
					if(data[i].tgt_sent_int_i.size()>max_target_minibatch) {
						max_target_minibatch = data[i].tgt_sent_int_i.size();
					}
				}

				for(int i=curr_index; i<std::min((int)data.size(),curr_index+minibatch_size); i++) {
					while(data[i].tgt_sent_int_i.size()<=max_target_minibatch) {
						data[i].tgt_sent_int_i.push_back(-1);
						data[i].tgt_sent_int_o.push_back(-1);
					}
				}
				curr_index+=minibatch_size;
			}
		}

		for(int i=0; i<data.size(); i++) {

			final_output << "\n";
			final_output << "\n";

			for(int j=0; j<data[i].tgt_sent_int_i.size(); j++) {
				final_output << data[i].tgt_sent_int_i[j];
				if(j!=data[i].tgt_sent_int_i.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";


			for(int j=0; j<data[i].tgt_sent_int_o.size(); j++) {
				final_output << data[i].tgt_sent_int_o[j];
				if(j!=data[i].tgt_sent_int_o.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}


		weights_file.close();
		final_output.close();
		target_input.close();

		std::cout << "----------------------------target dev file info-----------------------------\n";
		std::cout << "Number of target word tokens: " << VISUAL_num_target_word_tokens <<"\n";
		std::cout << "Number of target segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Longest target segment (after removing long sentences for training): " << VISUAL_target_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff: " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n";
	}

	void integerize_file_kbest(std::string output_weights_name,std::string source_file_name,std::string tmp_output_name,
		int max_sent_cutoff,int &hiddenstate_size,int &target_vocab_size,int &source_vocab_size,int &num_layers) 
	{


		int VISUAL_num_source_word_tokens =0;
		int VISUAL_num_segment_pairs=0;
		int VISUAL_source_longest_sent=0;

		int VISUAL_num_tokens_thrown_away=0;

		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());

		std::vector<std::string> file_input_vec;
		std::string str;
		std::string word;

		std::getline(weights_file, str);
		std::istringstream iss(str, std::istringstream::in);
		while(iss >> word) {
			file_input_vec.push_back(word);
		}

		if(file_input_vec.size()!=4) {
			std::cout << "ERROR: Neural network file format has been corrupted\n";
			exit (EXIT_FAILURE);
		}

		num_layers = std::stoi(file_input_vec[0]);
		hiddenstate_size = std::stoi(file_input_vec[1]);
		target_vocab_size = std::stoi(file_input_vec[2]);
		source_vocab_size = std::stoi(file_input_vec[3]);

		//now get the source mappings
		std::getline(weights_file, str); //get this line, since all equals
		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}

			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			src_mapping[word] = tmp_index;
		}

		//now that we have the mappings, integerize the file
		std::ofstream final_output;
		final_output.open(tmp_output_name.c_str());
		std::ifstream source_input;
		source_input.open(source_file_name.c_str());

		//first get the number of lines the the files and check to be sure they are the same
		int source_len = 0;
		std::string src_str;

		source_input.clear();

		source_input.seekg(0, std::ios::beg);
		while(std::getline(source_input, src_str)) {
			source_len++;
		}

		VISUAL_num_segment_pairs = source_len;

		source_input.clear();
		source_input.seekg(0, std::ios::beg);
		for(int i=0; i<source_len; i++) {
			std::vector<std::string> src_sentence;
			std::vector<std::string> tgt_sentence;
			std::getline(source_input, src_str);

			std::istringstream iss_src(src_str, std::istringstream::in);
			while(iss_src>> word) {
				src_sentence.push_back(word);
			}

			std::reverse(src_sentence.begin(),src_sentence.end());
			if( !(src_sentence.size()+1>=max_sent_cutoff-2) ) {
				data.push_back(comb_sent_info(src_sentence,tgt_sentence));
				VISUAL_num_source_word_tokens+=src_sentence.size();
				if(VISUAL_source_longest_sent < src_sentence.size()) {
					VISUAL_source_longest_sent = src_sentence.size();
				}
			}
			else {
				VISUAL_num_tokens_thrown_away+=src_sentence.size() + tgt_sentence.size();
			}
		}

		//now integerize
		for(int i=0; i<data.size(); i++) {
			std::vector<int> src_int;

			for(int j=0; j<data[i].src_sent.size(); j++) {
				if(src_mapping.count(data[i].src_sent[j])==0) {
					//tgt_int.push_back(target_vocab_size-1);
					src_int.push_back(src_mapping["<UNK>"]);
				}
				else {
					src_int.push_back(src_mapping[data[i].src_sent[j]]);
				}	
			}

			data[i].src_sent.clear();
			data[i].src_sent_int= src_int;
			data[i].src_sent_int.insert(data[i].src_sent_int.begin(),0);
		}

		for(int i=0; i<data.size(); i++) {
			for(int j=0; j<data[i].src_sent_int.size(); j++) {
				final_output << data[i].src_sent_int[j];
				if(j!=data[i].src_sent_int.size()) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}


		weights_file.close();
		final_output.close();
		source_input.close();

		std::cout << "----------------------------source kbest file info-----------------------------\n";
		std::cout << "Number of source word tokens: " << VISUAL_num_source_word_tokens <<"\n";
		std::cout << "Number of source segment pairs (lines in training file): " << VISUAL_num_segment_pairs<<"\n";
		std::cout << "Longest source segment (after removing long sentences for training): " << VISUAL_source_longest_sent << "\n";
		std::cout << "Total word tokens thrown out due to sentence cutoff: " << VISUAL_num_tokens_thrown_away <<"\n";
		std::cout << "-------------------------------------------------------------------------------\n\n";


	}

	//need outputweights to get the int mapping
	void unint_file(std::string output_weights_name,std::string unint_file,std::string output_final_name,bool LM,bool decoder) {

		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());
		weights_file.clear();
		weights_file.seekg(0, std::ios::beg);

		std::string str;
		std::string word;

		std::getline(weights_file, str); //info from first sentence
		std::getline(weights_file, str); //======== stuff
		if(!LM) {
			while(std::getline(weights_file, str)) {
				if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
					break; //done with target mapping
				}
			}
		}

		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}
			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_reverse_mapping[tmp_index] = word;
		}


		weights_file.close();

		std::ifstream unint;
		unint.open(unint_file.c_str());

		std::ofstream final_output;
		final_output.open(output_final_name.c_str());

		while(std::getline(unint, str)) {
			std::istringstream iss(str, std::istringstream::in);
			std::vector<int> sent_int;

			if(decoder) {
				if(str[0]=='-'|| str[0] == ' ' || str.size()==0) {
					final_output << str << "\n";
					continue;
				}
			}

			while(iss >> word) {
				sent_int.push_back(std::stoi(word));
			}

			for(int i=0; i<sent_int.size(); i++) {
				final_output << tgt_reverse_mapping[sent_int[i]];
				if(i!=sent_int.size()-1) {
					final_output << " ";
				}
			}
			final_output << "\n";
		}

		final_output.close();
		unint.close();
	}


	void unint_alignments(std::string output_weights_name,std::string int_alignments_file,std::string final_alignment_file) {
		std::ifstream weights_file;
		weights_file.open(output_weights_name.c_str());
		weights_file.clear();
		weights_file.seekg(0, std::ios::beg);

		std::string str;
		std::string word;

		std::getline(weights_file, str); //info from first sentence
		std::getline(weights_file, str); //======== stuff

		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}
			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			src_reverse_mapping[tmp_index] = word;
		}

		while(std::getline(weights_file, str)) {
			int tmp_index;

			if(str.size()>3 && str[0]=='=' && str[1]=='=' && str[2]=='=') {
				break; //done with target mapping
			}
			std::istringstream iss(str, std::istringstream::in);
			iss >> word;
			tmp_index = std::stoi(word);
			iss >> word;
			tgt_reverse_mapping[tmp_index] = word;
		}


		weights_file.close();

		std::ifstream unint;
		unint.open(int_alignments_file.c_str());

		std::ofstream final_output;
		final_output.open(final_alignment_file.c_str());

		//goes source the target, so get source from the loop
		while(std::getline(unint, str)) {

			std::istringstream iss(str, std::istringstream::in);
			std::vector<int> sent_int_src;
			std::vector<int> sent_int_tgt;

			while(iss >> word) {
				sent_int_src.push_back(std::stoi(word));
			}

			//now the stuff for the target
			std::getline(unint, str);
			std::istringstream iss_2(str, std::istringstream::in);


			while(iss_2 >> word) {
				sent_int_tgt.push_back(std::stoi(word));
			}

			for(int i=0; i<sent_int_src.size(); i++) {
				final_output << src_reverse_mapping[sent_int_src[i]];
				if(i!=sent_int_src.size()-1) {
					final_output << " ";
				}
			}
			final_output << "\n";

			for(int i=0; i<sent_int_tgt.size(); i++) {
				final_output << tgt_reverse_mapping[sent_int_tgt[i]];
				if(i!=sent_int_tgt.size()-1) {
					final_output << " ";
				}
			}
			final_output << "\n";

		}

		final_output.close();
		unint.close();
	}


};



#endif