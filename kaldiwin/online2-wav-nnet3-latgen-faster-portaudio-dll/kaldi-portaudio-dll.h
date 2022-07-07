#pragma once
#include "feat/wave-reader.h"
#include "online/online-audio-source.h"
#include "online2/online-nnet3-decoding.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "decoder/lattice-faster-decoder.h"
#include "fstext/fstext-lib.h"
#include "fstext/fstext-utils.h"
#include "lat/lattice-functions.h"
#include "util/kaldi-thread.h"
#include "nnet3/nnet-utils.h"
#include <locale.h>
#include <windows.h>
#include <iostream>
#include "lat/confidence.h"
#include "lat/kaldi-lattice.h"
#include "lat/word-align-lattice.h"
#include <fst/lookahead-matcher.h>
#include <fst/extensions/ngram/ngram-fst.h>

#include "kaldi-portaudio-dll.h"

#ifdef ONLINE2WAVNNET3LATGENFASTERPORTAUDIODLL_EXPORTS
#define DLLRECOGNITION_API __declspec(dllexport)
#else
#define DLLRECOGNITION_API __declspec(dllimport)
#endif
// DLL internal state variables:
static unsigned long long previous_;  // Previous value, if any
static unsigned long long current_;   // Current sequence value
static unsigned index_;               // Current seq. position



//using namespace std::placeholders;
using namespace std;
//using namespace kaldi;
#define MAX_SH_MEM_SIZE 2048   //1k, Size of Shared Memory
#define MAX_VIEW_SIZE 1024 
#define WAIT_TIME_OUT 100

static HANDLE  g_hSharedMemory = NULL; //global handle to shared memory
static LPTSTR  g_pBuffer = NULL;       //shared memory pointer
static WCHAR   g_szShareMemoryName[] = (L"KaldiShMemory");  //Name of the shared memory, required for a Named Kernal Object

static HANDLE g_hMutex = NULL;
static WCHAR  g_szMutexName[] = (L"KaldiMutex");  //Name of the mutexy, required for a Named Kernel Object
							
static const int32 kSampleFreq = 8000; // Input sampling frequency is fixed to 8KHz
static int32 chunk_length;


namespace kaldi {
	static bool do_endpointing = false;
	static bool online = true;
	static OnlineEndpointConfig endpoint_opts;
	//static SingleUtteranceNnet3Decoder* decoder_;
	//static OnlineNnet2FeaturePipeline* feature_pipeline_;
	//static OnlineSilenceWeighting* silence_weighting_;
	//static OnlineIvectorExtractorAdaptationState* adaptation_state_;
	static Matrix<double> global_cmvn_stats;
	static TransitionModel trans_model;
	static nnet3::AmNnetSimple am_nnet;
	static nnet3::DecodableNnetSimpleLoopedInfo* decodable_info;
	static nnet3::NnetSimpleLoopedComputationOptions decodable_opts;
	static LatticeFasterDecoderConfig decoder_opts;
	static OnlineNnet2FeaturePipelineInfo* feature_info;
	static fst::Fst<fst::StdArc> *decode_fst;
	static fst::LookaheadFst<fst::StdArc, int32> *decode_fst_ = nullptr;
	static fst::StdOLabelLookAheadFst *hcl_fst_ = nullptr; //StdOLabelLookAheadFst
	static fst::NGramFst<fst::StdArc> *g_fst_ = nullptr;
	static vector<int32> disambig_;
	static const fst::SymbolTable *word_syms = NULL;
	static kaldi::WordBoundaryInfo *winfo_ = nullptr;
	static OnlinePaSource *au_src;
	//static wchar_t * output_str;
extern "C" DLLRECOGNITION_API void init();
extern "C" DLLRECOGNITION_API void init_kaldi_nnet();
extern "C" DLLRECOGNITION_API const wchar_t* recognize_wav(wchar_t* path);
extern "C" DLLRECOGNITION_API const wchar_t* recognize_mic();

void GetDiagnosticsAndPrintOutput(const std::string &utt,
	const fst::SymbolTable *word_syms,
	const CompactLattice &clat,
	int64 *tot_num_frames,
	double *tot_like);

}


// Produce the next value in the sequence.
// Returns true on success and updates current value and index;
// false on overflow, leaves current value and index unchanged.
//extern "C" DLLRECOGNITION_API bool fibonacci_next();

// Get the current value in the sequence.
//extern "C" DLLRECOGNITION_API unsigned long long fibonacci_current();

// Get the position of the current value in the sequence.
//extern "C" DLLRECOGNITION_API unsigned fibonacci_index();