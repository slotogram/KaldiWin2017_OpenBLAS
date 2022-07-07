//#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier
#include <utility>
#include <limits.h>
#include "kaldi-portaudio-dll.h"
#include <iostream>
#include <thread>
#include <windows.h>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <chrono> 
#include <Combaseapi.h>

namespace kaldi
{
	std::wstring string_to_wstring(const string str) { //TODO: check if buf and sss not needed
		std::wstring convertedString;
		int requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, 0, 0);

		if (requiredSize > 0) {
			std::vector<wchar_t> buffer(requiredSize);
			MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &buffer[0], requiredSize);
			convertedString.assign(buffer.begin(), buffer.end() - 1);
		}
		//delete buf, sss;
		return convertedString;
	}

	std::string encode_1251(const std::wstring &wstr)
	{
		//if (wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(1251, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(1251, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}


	void GetDiagnosticsAndPrintOutput(const std::string &utt,
		const fst::SymbolTable *word_syms,
		const CompactLattice &clat) {
		if (clat.NumStates() == 0) {
			KALDI_WARN << "Empty lattice.";
			return;
		}
		CompactLattice best_path_clat;
		CompactLatticeShortestPath(clat, &best_path_clat);
		Lattice best_path_lat;
		ConvertLattice(best_path_clat, &best_path_lat);
		double likelihood;
		LatticeWeight weight;
		int32 num_frames;
		std::vector<int32> alignment;
		std::vector<int32> words;
		GetLinearSymbolSequence(best_path_lat, &alignment, &words, &weight);
		num_frames = alignment.size();
		likelihood = -(weight.Value1() + weight.Value2());
		KALDI_VLOG(2) << "Likelihood per frame for utterance " << utt << " is "
			<< (likelihood / num_frames) << " over " << num_frames
			<< " frames.";
		//std::ofstream out;          // поток для записи
		//out.open("hello.txt");
		if (word_syms != NULL) {
			std::wstring sw_all = L" ";
			for (size_t i = 0; i < words.size(); i++) {
				std::string s = word_syms->Find(words[i]);
				if (s == "")
					KALDI_ERR << "Word-id " << words[i] << " not in symbol table.";

				//  out << s << ' '; 
				//s = encode_1251(string_to_wstring(s)); // kaldi формирует строку в utf-8, но система не распознает ее как utf-8. 
												   // как эти изменения отразятся, если тут будет не русский текст???
				//std::cerr << "before str to wstr\n";
				std::wstring sw = string_to_wstring(s);
				//std::cerr << s << ' ';
				//std::cerr << "after str to wstr\n";
				//std::cout << "send output recognition string" << std::endl;
				if (i == 0) 
					sw_all = sw;
				else
					sw_all = sw_all + L" " + sw;
				

				
			}
			char *mappedDataAsUChars = (char*)g_pBuffer;
			wchar_t *mappedDataAsUCharsW = (wchar_t*)g_pBuffer;
			const wchar_t* ret = sw_all.c_str();
			size_t bufsize = wcslen(ret) + 1;
			//std::cout << bufsize;
			//wchar_t* buffer = (wchar_t*)CoTaskMemAlloc(bufsize * sizeof(wchar_t));
			//std::cerr << "trying to write str len\n";
			mappedDataAsUChars[1] = ((char)bufsize - 1) * 2;
			//g_pBuffer[1] = L'2';
			//std::cerr << "trying to write str len2\n";
			//g_pBuffer[1] = (char)100;
			//std::cerr << "  " << mappedDataAsUChars[1];
			//std::cerr << "trying to write str\n";
			wcscpy_s(mappedDataAsUCharsW + 1, bufsize, ret);
			ReleaseMutex(g_hMutex);
			//std::cerr << std::endl;
		}
		//  out.close();
	}

	bool InitializeMemory()
	{
		if (g_hSharedMemory == NULL) { // check if already initialized
			g_hSharedMemory = CreateFileMapping(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security 
				PAGE_READWRITE,          // read/write access
				0,                       // max. object size 
				MAX_SH_MEM_SIZE,         // buffer size  
				g_szShareMemoryName);    // name of mapping object

			if (NULL == g_hSharedMemory ||
				INVALID_HANDLE_VALUE == g_hSharedMemory)
			{
				std::cerr << "Error occured while"
					" creating file mapping object :"
					<< GetLastError() << "\n";
				return false;
			}
			/*g_hSharedMemory = OpenFileMapping(FILE_MAP_ALL_ACCESS, 1, g_szShareMemoryName);
			if (NULL == g_hSharedMemory ||
				INVALID_HANDLE_VALUE == g_hSharedMemory)
			{
				std::cerr << "Error occured while"
					" creating file mapping object :"
					<< GetLastError() << "\n";
				return false;
			}*/
			g_pBuffer =
				(LPTSTR)MapViewOfFile(g_hSharedMemory, // handle to map object
					FILE_MAP_ALL_ACCESS, // read/write permission
					0,
					0,
					MAX_VIEW_SIZE);

			if (NULL == g_pBuffer)
			{ /*
				cout << "Error occured while mapping"
					" view of the file :" << GetLastError()
					<< "\n";*/
				return false;
			}
			g_hMutex = CreateMutex(NULL, FALSE, g_szMutexName);
			while (g_hMutex == NULL)
			{
				g_hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, g_szMutexName);
				std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME_OUT));
			}
		}
	}
	void DeInitialize()
	{
		UnmapViewOfFile(g_pBuffer);
		ReleaseMutex(g_hMutex);
		CloseHandle(g_hSharedMemory);
		CloseHandle(g_hMutex);
	}


	class Application
	{
		std::mutex m_mutex;
		std::mutex m_waitReceive;
		std::condition_variable m_condVar;
		std::condition_variable m_condWaitReceive;
		bool m_bStartSignalReceived;
		bool m_bRecognitionFinished;
		bool m_bNetworkInitialized;
	public:
		Application()
		{
			m_bStartSignalReceived = false;
			m_bRecognitionFinished = true;
			m_bNetworkInitialized = false;
		}
		void waitForStart()
		{
			std::unique_lock<std::mutex> guard(m_mutex);
			std::unique_lock<std::mutex> mlock(m_waitReceive);
			guard.unlock();
			mlock.unlock();
			bool waitDisplayed = false;
			while (true)
			{
				// Make This Thread sleep for 1 Second
				std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIME_OUT));
				if (!waitDisplayed)
				{
					std::cout << "Waiting for network receive for start recognition" << std::endl;
					waitDisplayed = true;
				}
				WaitForSingleObject(g_hMutex, INFINITE);
				unsigned char *mappedDataAsUChars = (unsigned char*)g_pBuffer;
				if (mappedDataAsUChars[0] == L'1')
				{
					g_pBuffer[0] = L'0';
					waitDisplayed = false;
					// Lock The Data structure
					guard.try_lock();
					std::cout << "2nd thread locked" << std::endl;
					// Set the flag to true, means singal received to start recognition
					m_bStartSignalReceived = true;
					guard.unlock();
					std::cout << "2nd thread unlocked" << std::endl;
					// Notify the condition variable
					m_bRecognitionFinished = false;
					m_condVar.notify_all();

					// Wait until main thread finishes recognition
					mlock.lock();
					m_condWaitReceive.wait(mlock, std::bind(&Application::isRecognitionFinished, this));
					mlock.unlock();
				}
				ReleaseMutex(g_hMutex);
			}

		}
		bool isStartSignalReceived()
		{
			return m_bStartSignalReceived;
		}
		bool isRecognitionFinished()
		{
			return m_bRecognitionFinished;
		}
		void sendOutput(std::string output)
		{
			//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			std::cout << "send output recognition string" << std::endl;
			unsigned char *mappedDataAsUChars = (unsigned char*)g_pBuffer;
			mappedDataAsUChars[1] = 5;
			//std::memcpy(mappedDataAsUChars + 2, L"qwйцу", 5);
			mappedDataAsUChars[2] = L'Q';
			mappedDataAsUChars[3] = L'Й';
			mappedDataAsUChars[4] = L'а';
			mappedDataAsUChars[5] = L'Й';
			mappedDataAsUChars[6] = L'Q';

			//mappedDataAsUChars+2 = L"qwйцу";
			//WaitForSingleObject(g_hMutex, INFINITE); // dont need, because its already locked
			ReleaseMutex(g_hMutex);
		}
		void init_kaldi_nnet3()
		{
			using namespace kaldi;
			using namespace fst;
			using namespace std::chrono;
			typedef kaldi::int32 int32;
			typedef kaldi::int64 int64;
			
			if (word_syms != NULL)
			{
				cerr << "network already initialized" << endl;
				BaseFloat chunk_length_secs = 0.18;
				if (!online) {
					feature_info->ivector_extractor_info.use_most_recent_ivector = true;
					feature_info->ivector_extractor_info.greedy_ivector_extractor = true;
					chunk_length = std::numeric_limits<int32>::max();
				}
				else {
					feature_info->ivector_extractor_info.use_most_recent_ivector = false;
					feature_info->ivector_extractor_info.greedy_ivector_extractor = false;
					chunk_length = int32(feature_info->GetSamplingFrequency() * chunk_length_secs);
					if (chunk_length == 0) chunk_length = 1;
				}

				return;
				
			}
			cerr << "before waiting for mutex" << endl;
			WaitForSingleObject(g_hMutex, INFINITE);
			g_pBuffer[0] = L'2';


			const char *usage =
				"Reads in wav file(s) and simulates online decoding with neural nets\n"
				"(nnet3 setup), with optional iVector-based speaker adaptation and\n"
				"optional endpointing.  Note: some configuration values and inputs are\n"
				"set via config files whose filenames are passed as options\n"
				"\n"
				"Usage: online2-wav-nnet3-latgen-faster [options] <nnet3-in> <fst-in> "
				"<spk2utt-rspecifier> <wav-rspecifier> <lattice-wspecifier>\n"
				"The spk2utt-rspecifier can just be <utterance-id> <utterance-id> if\n"
				"you want to decode utterance by utterance.\n";

			ParseOptions po(usage);

			std::string word_syms_rxfilename;

			// feature_opts includes configuration for the iVector adaptation,
			// as well as the basic features.
			OnlineNnet2FeaturePipelineConfig feature_opts;
			
			
			endpoint_opts.silence_phones = "1:2:3:4:5:6:7:8:9:10";
			endpoint_opts.rule2.min_trailing_silence = 0.25;
			endpoint_opts.rule3.min_trailing_silence = 0.5;
			//endpoint_opts.
			BaseFloat chunk_length_secs = 0.18;
			

			po.Register("chunk-length", &chunk_length_secs,
				"Length of chunk size in seconds, that we process.  Set to <= 0 "
				"to use all input in one chunk.");
			po.Register("word-symbol-table", &word_syms_rxfilename,
				"Symbol table for words [for debug output]");
			po.Register("do-endpointing", &do_endpointing,
				"If true, apply endpoint detection");
			po.Register("online", &online,
				"You can set this to false to disable online iVector estimation "
				"and have all the data for each utterance used, even at "
				"utterance start.  This is useful where you just want the best "
				"results and don't care about online operation.  Setting this to "
				"false has the same effect as setting "
				"--use-most-recent-ivector=true and --greedy-ivector-extractor=true "
				"in the file given to --ivector-extraction-config, and "
				"--chunk-length=-1.");
			po.Register("num-threads-startup", &g_num_threads,
				"Number of threads used when initializing iVector extractor.");

			feature_opts.Register(&po);
			decodable_opts.Register(&po);
			decoder_opts.Register(&po);
			endpoint_opts.Register(&po);

			//
			int argc = 14;
			char *argv[] = { (char*)"foo.exe",
				//(char*)"--do-endpointing=true",
				(char*)"--word-symbol-table=exp/tdnn/graph/words.txt",
				(char*)"--frame-subsampling-factor=3",
				(char*)"--frames-per-chunk=51",
				(char*)"--acoustic-scale=1.0",
				(char*)"--beam=12.0",
				(char*)"--lattice-beam=6.0",
				(char*)"--max-active=10000",
				(char*)"--config=exp/tdnn/conf/online.conf",
				(char*)"exp/tdnn/final.mdl",
				(char*)"exp/tdnn/graph/HCLG.fst",
				(char*)"ark:decoder-test.utt2spk",
				(char*)"scp:decoder-test.scp",
				(char*)"ark:-",
				NULL };

			po.Read(argc, argv);

			if (po.NumArgs() != 5) {
				po.PrintUsage();
				return;
			}

			std::string nnet3_rxfilename = po.GetArg(1),
				fst_rxfilename = po.GetArg(2),
				spk2utt_rspecifier = po.GetArg(3),
				wav_rspecifier = po.GetArg(4),
				clat_wspecifier = po.GetArg(5);
			cerr << "args readed" << endl;
			feature_info = new OnlineNnet2FeaturePipelineInfo(feature_opts);
			

			if (!online) {
				feature_info->ivector_extractor_info.use_most_recent_ivector = true;
				feature_info->ivector_extractor_info.greedy_ivector_extractor = true;
				chunk_length_secs = -1.0;
			}

			if (chunk_length_secs > 0) {
				chunk_length = int32(feature_info->GetSamplingFrequency() * chunk_length_secs);
				if (chunk_length == 0) chunk_length = 1;
			}
			else {
				chunk_length = std::numeric_limits<int32>::max();
			}

			/*if (feature_info->global_cmvn_stats_rxfilename != "")
				ReadKaldiObject(feature_info->global_cmvn_stats_rxfilename,
					&global_cmvn_stats);*/

			//TransitionModel trans_model;
			//nnet3::AmNnetSimple am_nnet;
			{
				bool binary;
				Input ki(nnet3_rxfilename, &binary);
				trans_model.Read(ki.Stream(), binary);
				am_nnet.Read(ki.Stream(), binary);
				nnet3::SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
				nnet3::SetDropoutTestMode(true, &(am_nnet.GetNnet()));
				nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet.GetNnet()));
			}

			// this object contains precomputed stuff that is used by all decodable
			// objects.  It takes a pointer to am_nnet because if it has iVectors it has
			// to modify the nnet to accept iVectors at intervals.
			decodable_info = new nnet3::DecodableNnetSimpleLoopedInfo(decodable_opts,
				&am_nnet);

			const char* hcl_fst_rxfilename_ = "exp/tdnn/graph/HCLr.fst";
			const char* g_fst_rxfilename_ = "exp/tdnn/graph/Gr.fst";
			const char* disambig_rxfilename_ = "exp/tdnn/graph/disambig_tid.int";

			struct stat buffer;

			if (stat(fst_rxfilename.c_str(), &buffer) == 0) {
				decode_fst = ReadFstKaldiGeneric(fst_rxfilename);
			}
			else
			{
				//hcl_fst_ = fst::StdFst::Read(hcl_fst_rxfilename_);
				hcl_fst_ = fst::StdOLabelLookAheadFst::Read(hcl_fst_rxfilename_);
				
				g_fst_ = fst::NGramFst<fst::StdArc>::Read(g_fst_rxfilename_);
				ReadIntegerVectorSimple(disambig_rxfilename_, &disambig_);
				decode_fst_ = LookaheadComposeFst(*hcl_fst_, *g_fst_, disambig_);
				
			}

			
			if (g_fst_ && g_fst_->OutputSymbols()) {
				word_syms = g_fst_->OutputSymbols();
			}
			else
			//fst::SymbolTable *word_syms = NULL;
			if (word_syms_rxfilename != "")
				if (!(word_syms = fst::SymbolTable::ReadText(word_syms_rxfilename)))
					KALDI_ERR << "Could not read symbol table from file "
					<< word_syms_rxfilename;

			const char* winfo_rxfilename_ = "exp/tdnn/graph/phones/word_boundary.int";
			
			if (stat(winfo_rxfilename_, &buffer) == 0) {
				KALDI_LOG << "Loading winfo " << winfo_rxfilename_;
				kaldi::WordBoundaryInfoNewOpts opts;
				winfo_ = new kaldi::WordBoundaryInfo(opts, winfo_rxfilename_);
			}
			
			m_bNetworkInitialized = true;
			
		}
		OnlinePaSource* init_kaldi_portaudio()
		{
			
			do_endpointing = true;
			online = true;
			init_kaldi_nnet3();
			if (au_src == NULL) // check if already exists
			{
				// Timeout interval for the PortAudio source
				const int32 kTimeout = 500; // half second

				// PortAudio's internal ring buffer size in bytes
				const int32 kPaRingSize = 32768;
				// Report interval for PortAudio buffer overflows in number of feat. batches
				const int32 kPaReportInt = 4;
				//OnlinePaSource au_src(kTimeout, kSampleFreq, kPaRingSize, kPaReportInt);
				return new OnlinePaSource(kTimeout, feature_info->GetSamplingFrequency(), kPaRingSize, kPaReportInt);
				cerr <<"sample freq: " << feature_info->GetSamplingFrequency() << "\n";
			}
			else return au_src;
		}
		void mainTask()
		{
			//initialize kaldi recognizer
			//try {
				using namespace kaldi;
				using namespace fst;
				using namespace std::chrono;
				typedef kaldi::int32 int32;
				typedef kaldi::int64 int64;
				setlocale(LC_ALL, "Russian");
				//SetConsoleOutputCP(CP_UTF8);
				//SetConsoleCP(CP_UTF8);
				std::unique_lock<std::mutex> guard(m_waitReceive);
				std::unique_lock<std::mutex> mlock(m_mutex);
				guard.unlock();
				mlock.unlock();
				if (au_src == NULL)
					au_src = init_kaldi_portaudio();

				int32 num_done = 0, num_err = 0;
				int64 num_frames = 0;

				OnlineTimingStats timing_stats;

				
				std::cerr << std::endl;

				for (; ; ) {
					OnlineIvectorExtractorAdaptationState adaptation_state(
						feature_info->ivector_extractor_info);
					OnlineCmvnState cmvn_state(global_cmvn_stats);
					string utt = "test";
					while (true) {
						OnlineNnet2FeaturePipeline feature_pipeline(*feature_info);
						feature_pipeline.SetAdaptationState(adaptation_state);
						feature_pipeline.SetCmvnState(cmvn_state);

						OnlineSilenceWeighting silence_weighting(
							trans_model,
							feature_info->silence_weighting_config,
							decodable_opts.frame_subsampling_factor);

						
						SingleUtteranceNnet3Decoder decoder(decoder_opts, trans_model,
							*decodable_info,
							hcl_fst_ ? *decode_fst_ : *decode_fst, &feature_pipeline);

						//int32 samp_offset = 0;
						std::vector<std::pair<int32, BaseFloat> > delta_weights;
						ReleaseMutex(g_hMutex);
						//Wait for signal to start recognition
						mlock.lock();
						std::cerr << "Main thread locked before start signal received" << std::endl;
						// Start waiting for the Condition Variable to get signaled
						// Wait() will internally release the lock and make the thread to block
						// As soon as condition variable get signaled, resume the thread and
						// again acquire the lock. Then check if condition is met or not
						// If condition is met then continue else again go in wait.
						m_condVar.wait(mlock, std::bind(&Application::isStartSignalReceived, this));
						m_bStartSignalReceived = false;
						mlock.unlock();

						if (do_endpointing == false) //for reusing if wav reconition function was called
						{
							do_endpointing = true;
							online = true;
							init_kaldi_nnet3();
						}

						auto start = high_resolution_clock::now();
						std::cerr << "Started mic recognition" << std::endl;
						while (true) {

							// Prepare the input audio samples
							Vector<BaseFloat> wave_part(chunk_length);
							bool ans =au_src->Read(&wave_part);
							std::cerr << "Audio IN started" << std::endl;
							feature_pipeline.AcceptWaveform(feature_info->GetSamplingFrequency(), wave_part);

							if (silence_weighting.Active() &&
								feature_pipeline.IvectorFeature() != NULL) {
								silence_weighting.ComputeCurrentTraceback(decoder.Decoder());
								silence_weighting.GetDeltaWeights(feature_pipeline.NumFramesReady(),
									&delta_weights);
								feature_pipeline.IvectorFeature()->UpdateFrameWeights(delta_weights);
							}

							decoder.AdvanceDecoding();


							if (do_endpointing && decoder.EndpointDetected(endpoint_opts)) {
								feature_pipeline.InputFinished();
								auto stop1 = high_resolution_clock::now();
								std::cerr << "stop1 " << std::endl << duration_cast<milliseconds>(stop1 - start).count();
								break;
							}
						}
						std::cerr << '\'';
						decoder.FinalizeDecoding();

						CompactLattice clat;
						bool end_of_utterance = true;
						decoder.GetLattice(end_of_utterance, &clat);



						std::cerr << "finished mic recognition" << std::endl;
						GetDiagnosticsAndPrintOutput(utt, word_syms, clat);
						auto stop2 = high_resolution_clock::now();
						//sendOutput("12312312");
						guard.try_lock();
						//std::cout << "Main thread locked recognition finished" << std::endl;
						// Set the flag to true, means data is loaded
						m_bRecognitionFinished = true;
						guard.unlock();
						//std::cout << "Main thread unlocked recognition finished" << std::endl;
						// Notify the condition variable
						m_condWaitReceive.notify_all();

						std::cerr << "stop2 " << std::endl << duration_cast<milliseconds>(stop2 - start).count();
						//decoding_timer.OutputStats(&timing_stats);

						// In an application you might avoid updating the adaptation state if
						// you felt the utterance had low confidence.  See lat/confidence.h
						feature_pipeline.GetAdaptationState(&adaptation_state);
						feature_pipeline.GetCmvnState(&cmvn_state);
						// we want to output the lattice with un-scaled acoustics.
						//BaseFloat inv_acoustic_scale =
						//    1.0 / decodable_opts.acoustic_scale;
						//ScaleLattice(AcousticLatticeScale(inv_acoustic_scale), &clat);

						//clat_writer.Write(utt, clat);
						//KALDI_LOG << "Decoded utterance " << utt;
						num_done++;
					}
				}
				//timing_stats.Print(online); 

				/* KALDI_LOG << "Decoded " << num_done << " utterances, "
						  << num_err << " with errors.";
				KALDI_LOG << "Overall likelihood per frame was " << (tot_like / num_frames)
						  << " per frame over " << num_frames << " frames.";
				*/
				delete decode_fst;
				delete word_syms; // will delete if non-NULL.
				return;
			/*}
			catch (const std::exception& e) {
				std::cerr << e.what();
				return;
			}*/

		}
		wchar_t* recognize_wav(std::wstring path)
		{
			//delete output_str;
			wchar_t* output_str;
			//output_str = new wchar_t[255];
			//initialize kaldi recognizer
			//try {
				using namespace kaldi;
				using namespace fst;
				using namespace std::chrono;
				typedef kaldi::int32 int32;
				typedef kaldi::int64 int64;
				setlocale(LC_ALL, "Russian");
				//SetConsoleOutputCP(CP_UTF8);
				//SetConsoleCP(CP_UTF8);
				std::unique_lock<std::mutex> guard(m_waitReceive);
				std::unique_lock<std::mutex> mlock(m_mutex);
				guard.unlock();
				mlock.unlock();
				//OnlinePaSource *au_src = init_kaldi_portaudio();
				do_endpointing = false;
				online = false;
				cerr << "init nnet3" << endl;
				init_kaldi_nnet3();
				cerr << "finished init nnet3" << endl;
				int32 num_done = 0, num_err = 0;
				int64 num_frames = 0;

				OnlineTimingStats timing_stats;


				std::cerr << std::endl;
				std::string wav_rspecifier = "path";
			    std::wofstream out;          // поток для записи
				out.open(wav_rspecifier);
				out << L"test "<< path;
				out.close();
				wav_rspecifier = "scp:path";
				RandomAccessTableReader<WaveHolder> wav_reader(wav_rspecifier);

				{
					OnlineIvectorExtractorAdaptationState adaptation_state(
						feature_info->ivector_extractor_info);
					OnlineCmvnState cmvn_state(global_cmvn_stats);

					string utt = "test";
					{
						const WaveData &wave_data = wav_reader.Value(utt);
						// get the data for channel zero (if the signal is not mono, we only
						// take the first channel).
						SubVector<BaseFloat> data(wave_data.Data(), 0);

						OnlineNnet2FeaturePipeline feature_pipeline(*feature_info);
						feature_pipeline.SetAdaptationState(adaptation_state);
						feature_pipeline.SetCmvnState(cmvn_state);

						OnlineSilenceWeighting silence_weighting(
							trans_model,
							feature_info->silence_weighting_config,
							decodable_opts.frame_subsampling_factor);

						SingleUtteranceNnet3Decoder decoder(decoder_opts, trans_model,
							*decodable_info,
							hcl_fst_ ? *decode_fst_ : *decode_fst, &feature_pipeline);

						int32 samp_offset = 0;
						std::vector<std::pair<int32, BaseFloat> > delta_weights;
						
						auto start = high_resolution_clock::now();
						std::cerr << "Started recognition" << std::endl;
						BaseFloat samp_freq = wave_data.SampFreq();
						if (samp_freq != feature_info->GetSamplingFrequency())
						{
							std::cerr << "Warning: WAV and model sample frequency mismatch";
						}
						while (samp_offset < data.Dim()) {

							int32 samp_remaining = data.Dim() - samp_offset;
							int32 num_samp = chunk_length < samp_remaining ? chunk_length
								: samp_remaining;

							SubVector<BaseFloat> wave_part(data, samp_offset, num_samp);
							feature_pipeline.AcceptWaveform(samp_freq, wave_part);

							samp_offset += num_samp;
							if (samp_offset == data.Dim()) {
								// no more input. flush out last frames
								feature_pipeline.InputFinished();
							}
							if (silence_weighting.Active() &&
								feature_pipeline.IvectorFeature() != NULL) {
								silence_weighting.ComputeCurrentTraceback(decoder.Decoder());
								silence_weighting.GetDeltaWeights(feature_pipeline.NumFramesReady(),
									&delta_weights);
								feature_pipeline.IvectorFeature()->UpdateFrameWeights(delta_weights);
							}

							decoder.AdvanceDecoding();

						}
						std::cerr << '\'';
						decoder.FinalizeDecoding();

						CompactLattice clat;
						bool end_of_utterance = true;
						decoder.GetLattice(end_of_utterance, &clat);



						std::cerr << "finished mic recognition" << std::endl;
						GetDiagnosticsAndPrintOutput(utt, word_syms, clat);
						
						
						
						//wcscpy(, );
						wstring tmp((wchar_t*)g_pBuffer + 1);
						const wchar_t* ret = tmp.c_str();
						size_t bufsize = wcslen(ret) + 1;
						output_str = (wchar_t*)CoTaskMemAlloc(bufsize * sizeof(wchar_t));
						wcscpy_s(output_str, bufsize, ret);

						auto stop2 = high_resolution_clock::now();
						//sendOutput("12312312");
						
						std::cerr << "stop2 " << std::endl << duration_cast<milliseconds>(stop2 - start).count();
						//decoding_timer.OutputStats(&timing_stats);

						// In an application you might avoid updating the adaptation state if
						// you felt the utterance had low confidence.  See lat/confidence.h
						feature_pipeline.GetAdaptationState(&adaptation_state);
						feature_pipeline.GetCmvnState(&cmvn_state);
						// we want to output the lattice with un-scaled acoustics.
						//BaseFloat inv_acoustic_scale =
						//    1.0 / decodable_opts.acoustic_scale;
						//ScaleLattice(AcousticLatticeScale(inv_acoustic_scale), &clat);

						//clat_writer.Write(utt, clat);
						//KALDI_LOG << "Decoded utterance " << utt;
						num_done++;
					}
				}

				//delete decode_fst;
				//delete word_syms; // will delete if non-NULL.
				

				return output_str;
			/*}
			catch (const std::exception& e) {
				std::cerr << e.what();
				return NULL;
			}*/

		}
		wchar_t* recognize_mic()
		{
			wchar_t* output_str;
			//output_str = new wchar_t[255];
			//initialize kaldi recognizer
			//try {
				using namespace kaldi;
				using namespace fst;
				using namespace std::chrono;
				typedef kaldi::int32 int32;
				typedef kaldi::int64 int64;
				setlocale(LC_ALL, "Russian");
				//SetConsoleOutputCP(CP_UTF8);
				//SetConsoleCP(CP_UTF8);
				std::unique_lock<std::mutex> guard(m_waitReceive);
				std::unique_lock<std::mutex> mlock(m_mutex);
				guard.unlock();
				mlock.unlock();
				cerr << "init nnet3" << endl;
				au_src = init_kaldi_portaudio();
				cerr << "finished init nnet3" << endl;
				int32 num_done = 0, num_err = 0;
				int64 num_frames = 0;
				
				OnlineTimingStats timing_stats;

				{
					OnlineIvectorExtractorAdaptationState adaptation_state(
						feature_info->ivector_extractor_info);
					OnlineCmvnState cmvn_state(global_cmvn_stats);
					string utt = "test";
					{
						OnlineNnet2FeaturePipeline feature_pipeline(*feature_info);
						feature_pipeline.SetAdaptationState(adaptation_state);
						feature_pipeline.SetCmvnState(cmvn_state);

						OnlineSilenceWeighting silence_weighting(
							trans_model,
							feature_info->silence_weighting_config,
							decodable_opts.frame_subsampling_factor);

						
						SingleUtteranceNnet3Decoder decoder(decoder_opts, trans_model,
							*decodable_info,
							hcl_fst_ ? *decode_fst_ : *decode_fst, &feature_pipeline);

						//int32 samp_offset = 0;
						std::vector<std::pair<int32, BaseFloat> > delta_weights;

						auto start = high_resolution_clock::now();
						std::cerr << "Started recognition" << std::endl;
						while (true) {

							// Prepare the input audio samples
							Vector<BaseFloat> wave_part(chunk_length);
							bool ans = au_src->Read(&wave_part);
							std::cerr << "Audio IN started" << std::endl;
							feature_pipeline.AcceptWaveform(feature_info->GetSamplingFrequency(), wave_part);

							if (silence_weighting.Active() &&
								feature_pipeline.IvectorFeature() != NULL) {
								silence_weighting.ComputeCurrentTraceback(decoder.Decoder());
								silence_weighting.GetDeltaWeights(feature_pipeline.NumFramesReady(),
									&delta_weights);
								feature_pipeline.IvectorFeature()->UpdateFrameWeights(delta_weights);
							}

							decoder.AdvanceDecoding();


							if (do_endpointing && decoder.EndpointDetected(endpoint_opts)) {
								feature_pipeline.InputFinished();
								auto stop1 = high_resolution_clock::now();
								std::cerr << "stop1 " << std::endl << duration_cast<milliseconds>(stop1 - start).count();
								break;
							}
						}
						
						std::cerr << '\'';
						decoder.FinalizeDecoding();

						CompactLattice clat;
						bool end_of_utterance = true;
						decoder.GetLattice(end_of_utterance, &clat);
						std::cerr << "finished mic recognition" << std::endl;
						GetDiagnosticsAndPrintOutput(utt, word_syms, clat);
						wstring tmp((wchar_t*)g_pBuffer + 1);
						const wchar_t* ret = tmp.c_str();
						size_t bufsize = wcslen(ret) + 1;
						output_str = (wchar_t*)CoTaskMemAlloc(bufsize * sizeof(wchar_t));
						wcscpy_s(output_str, bufsize, ret);

						auto stop2 = high_resolution_clock::now();
						std::cerr << "stop2 " << std::endl << duration_cast<milliseconds>(stop2 - start).count();
						// In an application you might avoid updating the adaptation state if
						// you felt the utterance had low confidence.  See lat/confidence.h
						feature_pipeline.GetAdaptationState(&adaptation_state);
						feature_pipeline.GetCmvnState(&cmvn_state);
						num_done++;
						
					}
				}

				//delete decode_fst;
				//delete word_syms; // will delete if non-NULL.


				return output_str;
			/*}
			catch (const std::exception& e) {
				std::cerr << "Error during recognize_mic call: " << e.what();
				return NULL;
			}*/

		}
	};

	

	void init()
	{
		InitializeMemory();
		Application app;
		std::thread thread_1(&Application::mainTask, &app);
		std::thread thread_2(&Application::waitForStart, &app);
		thread_2.join();
		thread_1.join();
		DeInitialize(); 
	}
	const wchar_t* recognize_wav(wchar_t* path)
	{
		std::cerr << "Init mem " << std::endl;
		InitializeMemory();
		WaitForSingleObject(g_hMutex, INFINITE);
		Application app;
		std::cerr << "start main " << std::endl;
		wchar_t* ans = app.recognize_wav(std::wstring(path));
		std::cerr << "finishing " << std::endl;
		ReleaseMutex(g_hMutex);
		//DeInitialize();
		return ans;
	}
	const wchar_t* recognize_mic()
	{
		std::cerr << "Init mem " << std::endl;
		InitializeMemory();
		WaitForSingleObject(g_hMutex, INFINITE);
		Application app;
		std::cerr << "start main " << std::endl;
		wchar_t* ans = app.recognize_mic();
		std::cerr << "finishing " << std::endl;
		ReleaseMutex(g_hMutex);
		//DeInitialize();
		return ans;
	}
	void init_kaldi_nnet()
	{
		std::cerr << "Init mem " << std::endl;
		InitializeMemory();
		WaitForSingleObject(g_hMutex, INFINITE);
		Application app;
		au_src = app.init_kaldi_portaudio();
		//std::cerr << "start main " << std::endl;
		//wchar_t* ans = app.recognize_mic();
		std::cerr << "finishing " << std::endl;
		ReleaseMutex(g_hMutex);
	}
}