/*** written by Thomas Schoenemann 
 *** initially as a private person without employment, October 2009 
 *** later continued as an employee of Lund University, Sweden, 2010 - March 2011
 *** and later as a private person taking a time off, ***
 *** in small parts at the University of Pisa, Italy, October 2011 
 *** and at the University of Düsseldorf, Germany, 2012 ***/

#include "makros.hh"
#include "application.hh"
#include "corpusio.hh"
#include "ibm1_training.hh"
#include "ibm2_training.hh"
#include "hmm_training.hh"
#include "singleword_fertility_training.hh"
#include "ibm3_training.hh"
#include "ibm4_training.hh"
#include "timing.hh"
#include "training_common.hh"
#include "alignment_computation.hh"
#include "alignment_error_rate.hh"
#include "stringprocessing.hh"

#include <fstream>

#ifdef HAS_GZSTREAM
#include "gzstream.h"
#endif

int main(int argc, char** argv) {

  if (argc == 1 || strings_equal(argv[1],"-h")) {
    
    std::cerr << "USAGE: " << argv[0] << std::endl
              << " -s <file> : source file (coded as indices)" << std::endl
              << " -t <file> : target file (coded as indices)" << std::endl
              << " [-ds <file>] : additional source file (word indices) " << std::endl
              << " [-dt <file>] : additional target file (word indices) " << std::endl
              << " [-sclasses <file>] : source word classes (for IBM-4)" << std::endl 
              << " [-tclasses <file>] : target word classes (for IBM-4)" << std::endl 
              << " [-refa <file>] : file containing gold alignments (sure and possible)" << std::endl
              << " [-invert-biling-data] : switch source and target for prior dict and gold alignments" << std::endl
              << " [-method ( em | gd | viterbi )] : use EM, gradient descent or Viterbi training (default EM) " << std::endl
              << " [-dict-regularity <double>] : regularity weight for L0 or L1 regularization" << std::endl
              << " [-sparse-reg] : activate L1-regularity only for rarely occuring target words" << std::endl
              << " [-fertpen <double>]: regularity weight for fertilities in IBM3&4" << std::endl
              << " [-prior-dict <file>] : file for index pairs that occur in a dictionary" << std::endl
              << " [-l0-beta <double>] : smoothing parameter for the L0-norm in EM-training" << std::endl
              << " [-hmm-iter <uint> ]: iterations for the HMM model (default 20)" << std::endl
              << " [-ibm1-iter <uint> ]: iterations for the IBM-1 model (default 10)" << std::endl
              << " [-ibm2-iter <uint> ]: iterations for the IBM-2 model (default 0)" << std::endl
              << " [-ibm3-iter <uint> ]: iterations for the IBM-3 model (default 0)" << std::endl
              << " [-ibm4-iter <uint> ]: iterations for the IBM-4 model (default 0)" << std::endl
	      << " [-ibm4-mode (first | center | last) ] : (default first)" << std::endl
              << " [-constraint-mode (unconstrained | itg | ibm) " << std::endl
	      << " [-postdec-thresh <double>]" << std::endl
	      << " [-fert-limit <uint>]: fertility limit for IBM-3/4, default: 10000" << std::endl
	      << " [-hmm-type (fullpar | redpar | nonpar | nonpar2)]: default redpar" << std::endl
	      << " [-hmm-init-type (auto | par | nonpar | fix | fix2)]: default auto" << std::endl
              << " [-ibm1-transfer-mode (no | viterbi | posterior) ] : how to init HMM from IBM1, default: no" << std::endl
              << " [-count-collection] : do IBM-3 hillclimbing when initializing IBM-4" << std::endl
	      << " [-p0 <double>]: fix probability for empty alignments for IBM-3/4 " << std::endl
              << " [-org-empty-word] : for IBM 3/4 use empty word as originally published" << std::endl
              << " [-dont-reduce-deficiency] : use non-normalized probabilities for IBM-4 (as in Brown et al.)" << std::endl
              << " [-nonpar-distortion] : use extended set of distortion parameters for IBM-3" << std::endl
              << " [-dont-print-energy] : do not print the energy (speeds up EM for IBM-1 and HMM)" << std::endl
              << " [-max-lookup <uint>] : only store lookup tables up to this size. Default: 65535" << std::endl
              << " [-o <file>] : the determined dictionary is written to this file" << std::endl
              << " -oa <file> : the determined alignment is written to this file" << std::endl
              << std::endl;

    std::cerr << "this program estimates p(s|t)" << std::endl;;

    exit(0);
  }

  const int nParams = 36;
  ParamDescr  params[nParams] = {{"-s",mandInFilename,0,""},{"-t",mandInFilename,0,""},
                                 {"-ds",optInFilename,0,""},{"-dt",optInFilename,0,""},
                                 {"-o",optOutFilename,0,""},{"-oa",mandOutFilename,0,""},
                                 {"-refa",optInFilename,0,""},{"-invert-biling-data",flag,0,""},
                                 {"-dict-regularity",optWithValue,1,"0.0"},
                                 {"-sparse-reg",flag,0,""},{"-prior-dict",optInFilename,0,""},
                                 {"-hmm-iter",optWithValue,1,"5"},{"-method",optWithValue,1,"em"},
                                 {"-ibm1-iter",optWithValue,1,"5"},{"-ibm2-iter",optWithValue,1,"0"},
                                 {"-ibm3-iter",optWithValue,0,""},{"-ibm4-iter",optWithValue,0,""},
                                 {"-fertpen",optWithValue,1,"0.0"},{"-constraint-mode",optWithValue,1,"unconstrained"},
				 {"-l0-beta",optWithValue,1,"-1.0"},{"-ibm4-mode",optWithValue,1,"first"},
				 {"-fert-limit",optWithValue,1,"10000"},{"-postdec-thresh",optWithValue,1,"-1.0"},
                                 {"-hmm-type",optWithValue,1,"redpar"},{"-p0",optWithValue,1,"-1.0"},
                                 {"-org-empty-word",flag,0,""},{"-nonpar-distortion",flag,0,""},
                                 {"-hmm-init-type",optWithValue,1,"auto"},{"-dont-print-energy",flag,0,""},
                                 {"-ibm1-transfer-mode",optWithValue,1,"no"},{"-dict-struct",optWithValue,0,""},
                                 {"-dont-reduce-deficiency",flag,0,""},{"-count-collection",flag,0,""},
				 {"-sclasses",optInFilename,0,""},{"-tclasses",optInFilename,0,""},
                                 {"-max-lookup",optWithValue,1,"65535"}};

  Application app(argc,argv,params,nParams);

  NamedStorage1D<Storage1D<uint> > source_sentence(MAKENAME(source_sentence));
  NamedStorage1D<Storage1D<uint> > target_sentence(MAKENAME(target_sentence));

  NamedStorage1D<Storage1D<uint> > dev_source_sentence(MAKENAME(dev_source_sentence));
  NamedStorage1D<Storage1D<uint> > dev_target_sentence(MAKENAME(dev_target_sentence));  

  std::map<uint,std::set<std::pair<AlignBaseType,AlignBaseType> > > sure_ref_alignments;
  std::map<uint,std::set<std::pair<AlignBaseType,AlignBaseType> > > possible_ref_alignments;

  if (app.is_set("-refa")) {
    read_reference_alignment(app.getParam("-refa"), sure_ref_alignments, possible_ref_alignments,
                             app.is_set("-invert-biling-data"));
  }

  uint ibm1_iter = convert<uint>(app.getParam("-ibm1-iter")); 
  uint ibm2_iter = convert<uint>(app.getParam("-ibm2-iter"));
  uint hmm_iter = convert<uint>(app.getParam("-hmm-iter"));

  uint ibm3_iter = 0; 
  uint ibm4_iter = 0;

  if (app.is_set("-ibm3-iter"))
    ibm3_iter = convert<uint>(app.getParam("-ibm3-iter"));
  if (app.is_set("-ibm4-iter"))
    ibm4_iter = convert<uint>(app.getParam("-ibm4-iter"));

  std::string method = downcase(app.getParam("-method"));

  if (method != "em" && method != "gd" && method != "viterbi") {
    USER_ERROR << "unknown method \"" << method << "\"" << std::endl;
    exit(1);
  }

  double l0_fertpen = convert<double>(app.getParam("-fertpen"));

  double l0_beta = convert<double>(app.getParam("-l0-beta"));
  bool em_l0 = (l0_beta > 0);

  uint fert_limit = convert<uint>(app.getParam("-fert-limit"));

  const uint max_lookup = convert<uint>(app.getParam("-max-lookup"));

  double postdec_thresh = convert<double>(app.getParam("-postdec-thresh"));

  double fert_p0 = convert<double>(app.getParam("-p0"));

  std::clock_t tStartRead, tEndRead;
  tStartRead = std::clock();

  if (app.getParam("-s") == app.getParam("-t")) {

    std::cerr << "WARNING: files for source and target sentences are identical!" << std::endl;
  }

  read_monolingual_corpus(app.getParam("-s"), source_sentence);
  read_monolingual_corpus(app.getParam("-t"), target_sentence);

  if (app.is_set("-ds") != app.is_set("-dt")) {
    std::cerr << "WARNING: you need to specify both -ds and -dt . Ignoring additional sentences" << std::endl;
  }

  bool dev_present = app.is_set("-ds") && app.is_set("-dt");

  if (dev_present) {

    if (app.getParam("-ds") == app.getParam("-dt")) {

      std::cerr << "WARNING: dev-files for source and target sentences are identical!" << std::endl;
    }

    if (app.getParam("-s") == app.getParam("-ds")) {
      std::cerr << "WARNING: same file for source part of main corpus and development corpus" << std::endl;
    }
    if (app.getParam("-t") == app.getParam("-dt")) {
      std::cerr << "WARNING: same file for target part of main corpus and development corpus" << std::endl;
    }

    read_monolingual_corpus(app.getParam("-ds"), dev_source_sentence);
    read_monolingual_corpus(app.getParam("-dt"), dev_target_sentence);
  }

  tEndRead = std::clock();
  std::cerr << "reading the corpus took " << diff_seconds(tEndRead,tStartRead) << " seconds." << std::endl;

  assert(source_sentence.size() == target_sentence.size());

  uint nSentences = source_sentence.size();

  uint maxI = 0;
  uint maxJ = 0;

  for (size_t s=0; s < source_sentence.size(); s++) {

    uint curJ = source_sentence[s].size();
    uint curI = target_sentence[s].size();

    maxI = std::max<uint>(maxI,curI);
    maxJ = std::max<uint>(maxJ,curJ);

    if (9*curJ < curI || 9*curI < curJ) {

      std::cerr << "WARNING: GIZA++ would ignore sentence pair #" << (s+1) << ": J=" << curJ << ", I=" << curI << std::endl;
    }

    if (curJ == 0 || curI == 0) {
      USER_ERROR << "empty sentences are not allowed. Clean up your data!. Exiting." << std::endl;
      exit(1);
    }
  }

  if (maxJ > 254 || maxI > 254) {
    USER_ERROR << " maximum sentence length is 254. Clean up your data!. Exiting." << std::endl;
    exit(1);
  }


  uint nSourceWords = 0;
  uint nTargetWords = 0;

  for (size_t s=0; s < source_sentence.size(); s++) {

    for (uint k=0; k < source_sentence[s].size(); k++) {
      uint sidx = source_sentence[s][k];
      if (sidx == 0) {
        USER_ERROR << " index 0 is reserved for the empty word. Exiting.." << std::endl;
        exit(1);
      }
      nSourceWords = std::max(nSourceWords,sidx+1);
    }

    for (uint k=0; k < target_sentence[s].size(); k++) {
      uint tidx = target_sentence[s][k];
      if (tidx == 0) {
        USER_ERROR << " index 0 is reserved for the empty word. Exiting.." << std::endl;
        exit(1);
      }
      nTargetWords = std::max(nTargetWords,tidx+1);
    }
  }
  for (size_t s=0; s < dev_source_sentence.size(); s++) {

    for (uint k=0; k < dev_source_sentence[s].size(); k++) {
      uint sidx = dev_source_sentence[s][k];
      if (sidx == 0) {
        USER_ERROR << " index 0 is reserved for the empty word. Exiting.." << std::endl;
        exit(1);
      }
      nSourceWords = std::max(nSourceWords,dev_source_sentence[s][k]+1);
    }

    for (uint k=0; k < dev_target_sentence[s].size(); k++) {
      uint tidx = dev_target_sentence[s][k];
      if (tidx == 0) {
        USER_ERROR << " index 0 is reserved for the empty word. Exiting.." << std::endl;
        exit(1);
      }      
      nTargetWords = std::max(nTargetWords,dev_target_sentence[s][k]+1);
    }
  }


  CooccuringWordsType wcooc(MAKENAME(wcooc));
  CooccuringLengthsType lcooc(MAKENAME(lcooc));
  SingleWordDictionary dict(MAKENAME(dict));
  ReducedIBM2AlignmentModel reduced_ibm2align_model(MAKENAME(reduced_ibm2align_model));
  FullHMMAlignmentModel hmmalign_model(MAKENAME(hmmalign_model));
  InitialAlignmentProbability initial_prob(MAKENAME(initial_prob));

  Math1D::Vector<double> source_fert;

  Math1D::Vector<double> hmm_init_params;
  Math1D::Vector<double> hmm_dist_params;
  double hmm_dist_grouping_param = -1.0;

  double dict_regularity = convert<double>(app.getParam("-dict-regularity"));

  std::string hmm_string = downcase(app.getParam("-hmm-type"));
  if (hmm_string != "redpar" && hmm_string != "fullpar" 
      && hmm_string != "nonpar" && hmm_string != "nonpar2") {
    std::cerr << "WARNING: \"" << hmm_string << "\" is not a valid hmm type. Selecting redpar." << std::endl;
    hmm_string = "redpar";
  }

  HmmAlignProbType hmm_align_mode = HmmAlignProbReducedpar;
  if (hmm_string == "fullpar")
    hmm_align_mode = HmmAlignProbFullpar;
  else if (hmm_string == "nonpar") 
    hmm_align_mode = HmmAlignProbNonpar;
  else if (hmm_string == "nonpar2") 
    hmm_align_mode = HmmAlignProbNonpar2;

  HmmInitProbType hmm_init_mode = HmmInitPar;
  if (method == "viterbi")
    hmm_init_mode = HmmInitFix2;
      
  std::string hmm_init_string = downcase(app.getParam("-hmm-init-type"));
  if (hmm_init_string != "auto" && hmm_init_string != "par" && hmm_init_string != "nonpar" 
      && hmm_init_string != "fix" && hmm_init_string != "fix2") {
    std::cerr << "WARNING: \"" << hmm_init_string << "\" is not a valid hmm init type. Selecting auto." << std::endl;
    hmm_init_string = "auto";
  }
  if (hmm_init_string == "par")
    hmm_init_mode = HmmInitPar;
  else if (hmm_init_string == "nonpar")
    hmm_init_mode = HmmInitNonpar;
  else if (hmm_init_string == "fix")
    hmm_init_mode = HmmInitFix;
  else if (hmm_init_string == "fix2")
    hmm_init_mode = HmmInitFix2;

  std::cerr << "finding cooccuring words" << std::endl;
  bool read_in = false;
  if (app.is_set("-dict-struct")) {

    read_in = read_cooccuring_words_structure(app.getParam("-dict-struct"), nSourceWords, nTargetWords, wcooc);
  }
  if (!read_in)
    find_cooccuring_words(source_sentence, target_sentence, dev_source_sentence, dev_target_sentence, 
                          nSourceWords, nTargetWords, wcooc);
  
  std::cerr << "generating lookup table" << std::endl;
  LookupTable slookup;
  generate_wordlookup(source_sentence, target_sentence, wcooc, nSourceWords, slookup, max_lookup);

    
  floatSingleWordDictionary prior_weight(nTargetWords, MAKENAME(prior_weight));
      
  if (app.is_set("-ibm3-iter"))
    ibm3_iter = convert<uint>(app.getParam("-ibm3-iter"));
  if (app.is_set("-ibm4-iter"))
    ibm4_iter = convert<uint>(app.getParam("-ibm4-iter"));

  Math1D::Vector<double> distribution_weight;

  std::set<std::pair<uint, uint> > known_pairs;
  if (app.is_set("-prior-dict"))
    read_prior_dict(app.getParam("-prior-dict"), known_pairs, app.is_set("-invert-biling-data"));
  
  for (uint i=0; i < nTargetWords; i++) {
    uint size = (i == 0) ? nSourceWords-1 : wcooc[i].size();
    prior_weight[i].resize(size,0.0);
  }
  
  if (known_pairs.size() > 0) {
    
    for (uint i=0; i < nTargetWords; i++)
      prior_weight[i].set_constant(dict_regularity);
    
    uint nIgnored = 0;
    
    std::cerr << "processing read list" << std::endl;
    
    for (std::set<std::pair<uint, uint> >::iterator it = known_pairs.begin(); it != known_pairs.end() ; it++) {
      
      uint tword = it->first;
      uint sword = it->second;
      
      if (tword >= wcooc.size()) {
        std::cerr << "tword out of range: " << tword << std::endl;
      }

      uint pos = std::lower_bound(wcooc[tword].direct_access(), wcooc[tword].direct_access() + wcooc[tword].size(), sword) - 
        wcooc[tword].direct_access();

      if (pos < wcooc[tword].size()) {
        prior_weight[tword][pos] = 0.0;
      }
      else {
        nIgnored++;
        //std::cerr << "WARNING: ignoring entry of prior dictionary" << std::endl;
      }
    }
    
    std::cerr << "ignored " << nIgnored << " entries of prior dictionary" << std::endl;
  }
  else {
    
    distribution_weight.resize(nTargetWords,0.0);
    
    if (!app.is_set("-sparse-reg")) {
      distribution_weight.set_constant(dict_regularity);
    }
    else {
      for (size_t s=0; s < target_sentence.size(); s++) {
	
        for (uint i=0; i < target_sentence[s].size(); i++) {
          distribution_weight[target_sentence[s][i]] += 1.0;
        }
      }
      
      uint cutoff = 6;
      
      uint nSparse = 0;
      for (uint i=0; i < nTargetWords; i++) {
        if (distribution_weight[i] >= cutoff+1) 
          distribution_weight[i] = 0.0;
        else {
          nSparse++;
          //std::cerr << "sparse word: " << distribution_weight[i] << std::endl;
          distribution_weight[i] = (cutoff+1) - distribution_weight[i];
        }
      }
      distribution_weight[0] = 0.0;
      distribution_weight *= dict_regularity;
      std::cerr << "reg_sum: " << distribution_weight.sum() << std::endl;
      std::cerr << nSparse << " sparse words" << std::endl;
    }
    
    for (uint i=0; i < nTargetWords; i++)
      prior_weight[i].set_constant(distribution_weight[i]);
  }


  /**** now start training *****/

  std::cerr << "starting IBM 1 training" << std::endl;

  /*** IBM-1 ***/

  IBM1Options ibm1_options(nSourceWords,nTargetWords,sure_ref_alignments, possible_ref_alignments);
  ibm1_options.nIterations_ = ibm1_iter;
  ibm1_options.smoothed_l0_ = em_l0;
  ibm1_options.l0_beta_ = l0_beta;
  ibm1_options.print_energy_ = !app.is_set("-dont-print-energy");

  if (method == "em") {

    train_ibm1(source_sentence, slookup, target_sentence, wcooc, dict, 
               prior_weight, ibm1_options);
  }
  else if (method == "gd") {

    train_ibm1_gd_stepcontrol(source_sentence, slookup, target_sentence, wcooc, dict, 
                              prior_weight, ibm1_options); 
  }
  else {

    ibm1_viterbi_training(source_sentence, slookup, target_sentence, wcooc, dict, 
                          prior_weight, ibm1_options);
  }

  /*** IBM-2 ***/

  if (ibm2_iter > 0) {
    
    find_cooccuring_lengths(source_sentence, target_sentence, lcooc);

    if (method == "em") {

      train_reduced_ibm2(source_sentence,  slookup, target_sentence, wcooc, lcooc,
                         nSourceWords, nTargetWords, reduced_ibm2align_model, dict, ibm2_iter,
                         sure_ref_alignments, possible_ref_alignments);
    }
    else if (method == "gd") {

      std::cerr << "WARNING: IBM-2 is not available with gradient descent" << std::endl;
      train_reduced_ibm2(source_sentence,  slookup, target_sentence, wcooc, lcooc,
                         nSourceWords, nTargetWords, reduced_ibm2align_model, dict, ibm2_iter,
                         sure_ref_alignments, possible_ref_alignments);
    }
    else {

      ibm2_viterbi_training(source_sentence, slookup, target_sentence, wcooc, lcooc, nSourceWords, nTargetWords, 
                            reduced_ibm2align_model, dict, ibm2_iter, sure_ref_alignments, possible_ref_alignments, 
                            prior_weight);
    }
  }

  /*** HMM ***/

  HmmOptions hmm_options(nSourceWords,nTargetWords,sure_ref_alignments, possible_ref_alignments);
  hmm_options.nIterations_ = hmm_iter;
  hmm_options.align_type_ = hmm_align_mode;
  hmm_options.init_type_ = hmm_init_mode;
  hmm_options.smoothed_l0_ = em_l0;
  hmm_options.l0_beta_ = l0_beta;
  hmm_options.print_energy_ = !app.is_set("-dont-print-energy");

  std::string ibm1_transfer_mode = downcase(app.getParam("-ibm1-transfer-mode"));
  if (ibm1_transfer_mode != "no" && ibm1_transfer_mode != "viterbi" && ibm1_transfer_mode != "posterior") {
    std::cerr << "WARNING: unknown mode \"" << ibm1_transfer_mode << "\" for transfer from IBM-1 to HMM. Selecting \"no\"" << std::endl;
    ibm1_transfer_mode = "no";
  }
  
  hmm_options.transfer_mode_ = IBM1TransferNo;
  if (ibm1_transfer_mode == "posterior")
    hmm_options.transfer_mode_ = IBM1TransferPosterior;
  else if (ibm1_transfer_mode == "viterbi")
    hmm_options.transfer_mode_ = IBM1TransferViterbi;

  if (method == "em") {

    train_extended_hmm(source_sentence, slookup, target_sentence, wcooc, 
                       hmmalign_model, hmm_dist_params, hmm_dist_grouping_param, source_fert,
                       initial_prob, hmm_init_params, dict, prior_weight, hmm_options);
  }
  else if (method == "gd") {

    train_extended_hmm_gd_stepcontrol(source_sentence, slookup, target_sentence, wcooc, 
                                      hmmalign_model, hmm_dist_params, hmm_dist_grouping_param, source_fert,
                                      initial_prob, hmm_init_params, dict, prior_weight, hmm_options); 
  }
  else {

    viterbi_train_extended_hmm(source_sentence, slookup, target_sentence, wcooc, 
                               hmmalign_model, hmm_dist_params, hmm_dist_grouping_param, source_fert,
                               initial_prob, hmm_init_params, dict, 
                               prior_weight, false, hmm_options);
  }
  
  /*** IBM-3 ***/

  std::cerr << "handling IBM-3" << std::endl;

  IBM3Trainer ibm3_trainer(source_sentence, slookup, target_sentence, 
                           sure_ref_alignments, possible_ref_alignments,
                           dict, wcooc, nSourceWords, nTargetWords, prior_weight, 
                           !app.is_set("-nonpar-distortion"), !app.is_set("-org-empty-word"), 
                           false, l0_fertpen, em_l0, l0_beta);

  ibm3_trainer.set_fertility_limit(fert_limit);
  if (fert_p0 >= 0.0)
    ibm3_trainer.fix_p0(fert_p0);

  if (ibm3_iter+ibm4_iter > 0)
    ibm3_trainer.init_from_hmm(hmmalign_model,initial_prob,hmm_options,method == "viterbi");

  if (ibm3_iter > 0) {

    if (method == "em" || method == "gd") {
      
      std::string constraint_mode = downcase(app.getParam("-constraint-mode"));

      if (constraint_mode == "unconstrained") {
        ibm3_trainer.train_unconstrained(ibm3_iter);
      }
      else if (constraint_mode == "itg") 
        ibm3_trainer.train_with_itg_constraints(ibm3_iter,true);
      else if (constraint_mode == "ibm") 
        ibm3_trainer.train_with_ibm_constraints(ibm3_iter,5,3);
      else {
        USER_ERROR << "unknown constraint mode: \"" << constraint_mode << "\". Exiting" << std::endl;
        exit(1);
      }
    }
    else
      ibm3_trainer.train_viterbi(ibm3_iter,false);
  
    if (ibm4_iter == 0 || !app.is_set("-count-collection"))
      ibm3_trainer.update_alignments_unconstrained();
  }

  /*** IBM-4 ***/

  std::cerr << "handling IBM-4" << std::endl;

  Storage1D<WordClassType> source_class(nSourceWords,0);
  Storage1D<WordClassType> target_class(nTargetWords,0);  

  if (app.is_set("-sclasses"))
    read_word_classes(app.getParam("-sclasses"),source_class);
  if (app.is_set("-tclasses"))
    read_word_classes(app.getParam("-tclasses"),target_class);

  IBM4CeptStartMode ibm4_cept_mode = IBM4FIRST;
  std::string ibm4_mode = app.getParam("-ibm4-mode");
  if (ibm4_mode == "first")
    ibm4_cept_mode = IBM4FIRST;
  else if (ibm4_mode == "center")
    ibm4_cept_mode = IBM4CENTER;
  else if (ibm4_mode == "last")
    ibm4_cept_mode = IBM4LAST;
  else {
    USER_ERROR << "unknown ibm4 mode: \"" << ibm4_mode << "\"" << std::endl;
    exit(1);
  }

  
  IBM4Trainer ibm4_trainer(source_sentence, slookup, target_sentence, 
                           sure_ref_alignments, possible_ref_alignments,
                           dict, wcooc, nSourceWords, nTargetWords, prior_weight, 
                           source_class, target_class, !app.is_set("-org-empty-word"), true, true,
                           !app.is_set("-dont-reduce-deficiency"), 
                           ibm4_cept_mode, em_l0, l0_beta, l0_fertpen);

  ibm4_trainer.set_fertility_limit(fert_limit);
  if (fert_p0 >= 0.0)
    ibm4_trainer.fix_p0(fert_p0);


  if (ibm4_iter > 0) {
    bool collect_counts = false;
    
    ibm4_trainer.init_from_ibm3(ibm3_trainer,true,collect_counts,method == "viterbi");
    
    if (collect_counts)
      ibm4_iter--;
    
    if (method == "viterbi")
      ibm4_trainer.train_viterbi(ibm4_iter);
    else
      ibm4_trainer.train_unconstrained(ibm4_iter);

    //ibm4_trainer.update_alignments_unconstrained();
  }

  LookupTable dev_slookup;
  if (dev_present) {
    generate_wordlookup(dev_source_sentence, dev_target_sentence, wcooc, nSourceWords, dev_slookup);
  }

  /*** write alignments ***/

  int max_devJ = 0;
  int max_devI = 0;

  std::set<uint> dev_seenIs;

  std::string dev_file = app.getParam("-oa") + ".dev";
  if (string_ends_with(app.getParam("-oa"),".gz"))
    dev_file += ".gz";

  if (dev_present) {
    for (size_t s = 0; s < dev_source_sentence.size(); s++) {

      const int curI = dev_target_sentence[s].size();
      const int curJ = dev_source_sentence[s].size();

      dev_seenIs.insert(curI);

      max_devJ = std::max(max_devJ,curJ);
      max_devI = std::max(max_devI,curI);	
    }
  }

  Math1D::Vector<double> dev_hmm_init_params(max_devI,0.0);
  Math1D::Vector<double> dev_hmm_dist_params(std::max(2*max_devI-1,0),0.0);
  FullHMMAlignmentModel dev_hmmalign_model(MAKENAME(dev_hmmalign_model));
  InitialAlignmentProbability dev_initial_prob(MAKENAME(dev_initial_prob));

  uint dev_zero_offset = max_devI - 1;

  if (dev_present) {
    
    uint train_zero_offset = maxI - 1;
    
    //handle case where init and/or distance parameters were not estimated above for <emph>train</emph>
    if (hmm_init_mode == HmmInitNonpar || hmm_init_params.sum() < 1e-5) {

      hmm_init_params.resize(maxI);
      hmm_init_params.set_constant(0.0);

      for (uint I=1; I <= maxI; I++) {
        
        for (uint l=0; l < std::min<uint>(I,initial_prob[I-1].size()); l++) {
          if (l < hmm_init_params.size()) 
            hmm_init_params[l] += initial_prob[I-1][l];
        }
      }
      
      double sum = hmm_init_params.sum();
      assert(sum > 0.0);
      hmm_init_params *= 1.0 / sum;
    }
    
    if (hmm_align_mode == HmmAlignProbNonpar || hmm_align_mode == HmmAlignProbNonpar2 || hmm_dist_params.sum() < 1e-5) {

      hmm_dist_grouping_param = -1.0;

      hmm_dist_params.resize(2*maxI-1);
      hmm_dist_params.set_constant(0.0);
      
      for (uint I=1; I <= maxI; I++) {
        
        for (uint i1 = 0; i1 < hmmalign_model[I-1].yDim(); i1++) 
          for (uint i2 = 0; i2 < I; i2++) 
            hmm_dist_params[train_zero_offset + i2 - i1] += hmmalign_model[I-1](i1,i2);
      }
      
      hmm_dist_params *= 1.0 / hmm_dist_params.sum();
    }

    if ((hmm_init_mode == HmmInitNonpar && hmm_align_mode == HmmAlignProbNonpar) 
        || source_fert.sum() < 0.95) {
      
      source_fert.resize(2);
      source_fert[0] = 0.02;
      source_fert[1] = 0.98;
    }

    for (uint i=0; i < std::min<uint>(max_devI,hmm_init_params.size()); i++) {
      dev_hmm_init_params[i] = hmm_init_params[i];	
      
      dev_hmm_dist_params[dev_zero_offset - i] = hmm_dist_params[train_zero_offset - i];
      dev_hmm_dist_params[dev_zero_offset + i] = hmm_dist_params[train_zero_offset + i];
    }

    dev_hmmalign_model.resize(max_devI+1);
    dev_initial_prob.resize(max_devI+1);

    for (std::set<uint>::iterator it = dev_seenIs.begin(); it != dev_seenIs.end(); it++) {
      
      uint I = *it;
      
      dev_hmmalign_model[I-1].resize(I+1,I,0.0); //because of empty words
      dev_initial_prob[I-1].resize(2*I,0.0);
    }

    if (hmm_init_mode != HmmInitFix && hmm_init_mode != HmmInitFix2) {
      par2nonpar_hmm_init_model(dev_hmm_init_params, source_fert, HmmInitPar, dev_initial_prob);
      if (hmm_init_mode == HmmInitNonpar) {
        for (uint I=0; I < std::min(dev_initial_prob.size(),initial_prob.size()); I++) {
          if (dev_initial_prob[I].size() > 0 && initial_prob[I].size() > 0)
            dev_initial_prob[I] = initial_prob[I];
        }
      }
    }
    else {
      par2nonpar_hmm_init_model(dev_hmm_init_params, source_fert, hmm_init_mode, dev_initial_prob);
    }

    HmmAlignProbType mode = hmm_align_mode;
    if (mode == HmmAlignProbNonpar || hmm_align_mode == HmmAlignProbNonpar2)
      mode = HmmAlignProbFullpar;
    
    par2nonpar_hmm_alignment_model(dev_hmm_dist_params, dev_zero_offset, hmm_dist_grouping_param, source_fert,
                                   mode, dev_hmmalign_model);
    
    if (hmm_align_mode == HmmAlignProbNonpar || hmm_align_mode == HmmAlignProbNonpar2) {
      
      for (uint I=0; I < std::min(dev_hmmalign_model.size(),hmmalign_model.size()); I++) {
        if (dev_hmmalign_model.size() > 0 && hmmalign_model.size() > 0)
          dev_hmmalign_model[I] = hmmalign_model[I];
      }
    }

    for (uint e=0; e < dict.size(); e++) {
      if (dict[e].sum() == 0.0)
        dict[e].set_constant(1e-5);
    }
  }
  
  if (ibm4_iter > 0) {
    if (postdec_thresh <= 0.0) 
      ibm4_trainer.write_alignments(app.getParam("-oa"));
    else
      ibm4_trainer.write_postdec_alignments(app.getParam("-oa"),postdec_thresh);

    if (dev_present) {
      
      std::cerr << "dev sentences present" << std::endl;
      
      Math1D::Vector<AlignBaseType> viterbi_alignment;
      std::set<std::pair<AlignBaseType,AlignBaseType> > postdec_alignment;

      std::ostream* dev_alignment_stream;
      
#ifdef HAS_GZSTREAM
      if (string_ends_with(app.getParam("-oa"),".gz")) {
	dev_alignment_stream = new ogzstream(dev_file.c_str());
      }
      else {
#else
      if (true) {
#endif
        dev_alignment_stream = new std::ofstream(dev_file.c_str());
      }

      for (size_t s = 0; s < dev_source_sentence.size(); s++) {
	
	const uint curI = dev_target_sentence[s].size();

	//initialize by HMM	
        compute_ehmm_viterbi_alignment(dev_source_sentence[s],dev_slookup[s], dev_target_sentence[s], 
                                       dict, dev_hmmalign_model[curI-1], dev_initial_prob[curI-1],
                                       viterbi_alignment, hmm_align_mode, false);
		
	if (postdec_thresh <= 0.0) {
	  
	  ibm4_trainer.compute_external_alignment(dev_source_sentence[s],dev_target_sentence[s],dev_slookup[s],
						  viterbi_alignment);
	    
	  for (uint j=0; j < viterbi_alignment.size(); j++) { 
	    if (viterbi_alignment[j] > 0)
	      (*dev_alignment_stream) << (viterbi_alignment[j]-1) << " " << j << " ";
	  }
	  
	  (*dev_alignment_stream) << std::endl;
	}
	else {
	  
	  ibm4_trainer.compute_external_postdec_alignment(dev_source_sentence[s],dev_target_sentence[s],dev_slookup[s],
							  viterbi_alignment, postdec_alignment, postdec_thresh);
	  
	  for(std::set<std::pair<AlignBaseType,AlignBaseType> >::iterator it = postdec_alignment.begin(); 
	      it != postdec_alignment.end(); it++) {
	    
	    (*dev_alignment_stream) << (it->second-1) << " " << (it->first-1) << " ";
	  }
	  (*dev_alignment_stream) << std::endl;
	}
      }
      delete dev_alignment_stream;
    }
  }
  else if (ibm3_iter > 0) {
    if (postdec_thresh <= 0.0) 
      ibm3_trainer.write_alignments(app.getParam("-oa"));
    else
      ibm3_trainer.write_postdec_alignments(app.getParam("-oa"),postdec_thresh);

    if (dev_present) {

      Math1D::Vector<AlignBaseType> viterbi_alignment;
      std::set<std::pair<AlignBaseType,AlignBaseType> > postdec_alignment;

      
      std::ostream* dev_alignment_stream;
      
#ifdef HAS_GZSTREAM
      if (string_ends_with(app.getParam("-oa"),".gz")) {
	dev_alignment_stream = new ogzstream(dev_file.c_str());
      }
      else {
#else
      if (true) {
#endif
        dev_alignment_stream = new std::ofstream(dev_file.c_str());
      }

      for (size_t s = 0; s < dev_source_sentence.size(); s++) {
	  
	const uint curI = dev_target_sentence[s].size();	

	//initialize by HMM	
        if (hmm_align_mode == HmmAlignProbReducedpar)
          compute_ehmm_viterbi_alignment_with_tricks(dev_source_sentence[s],dev_slookup[s], dev_target_sentence[s], 
                                                     dict, dev_hmmalign_model[curI-1], dev_initial_prob[curI-1],
                                                     viterbi_alignment, false);
        else
          compute_ehmm_viterbi_alignment(dev_source_sentence[s],dev_slookup[s], dev_target_sentence[s], 
                                         dict, dev_hmmalign_model[curI-1], dev_initial_prob[curI-1],
                                         viterbi_alignment, false);
	
	if (postdec_thresh <= 0.0) {

	  ibm3_trainer.compute_external_alignment(dev_source_sentence[s],dev_target_sentence[s],dev_slookup[s],
						  viterbi_alignment);
	  
	  for (uint j=0; j < viterbi_alignment.size(); j++) { 
	    if (viterbi_alignment[j] > 0)
	      (*dev_alignment_stream) << (viterbi_alignment[j]-1) << " " << j << " ";
	  }
	  
	  (*dev_alignment_stream) << std::endl;
	}
	else {
	  
	  ibm3_trainer.compute_external_postdec_alignment(dev_source_sentence[s],dev_target_sentence[s],dev_slookup[s],
							  viterbi_alignment, postdec_alignment, postdec_thresh);
	  
	  for(std::set<std::pair<AlignBaseType,AlignBaseType> >::iterator it = postdec_alignment.begin(); 
	      it != postdec_alignment.end(); it++) {
	    
	    (*dev_alignment_stream) << (it->second-1) << " " << (it->first-1) << " ";
	  }
	  (*dev_alignment_stream) << std::endl;  
	}
      }
      delete dev_alignment_stream;
    }  
  }
  else {

    std::ostream* alignment_stream;

#ifdef HAS_GZSTREAM
    if (string_ends_with(app.getParam("-oa"),".gz")) {
      alignment_stream = new ogzstream(app.getParam("-oa").c_str());
    }
    else {
#else
    if (true) {
#endif
      alignment_stream = new std::ofstream(app.getParam("-oa").c_str());
    }

    Storage1D<AlignBaseType> viterbi_alignment;
    std::set<std::pair<AlignBaseType,AlignBaseType> > postdec_alignment;

    for (size_t s = 0; s < nSentences; s++) {

      const uint curI = target_sentence[s].size();

      SingleLookupTable aux_lookup;
      const SingleLookupTable& cur_lookup = get_wordlookup(source_sentence[s],target_sentence[s],wcooc,
                                                           nSourceWords,slookup[s],aux_lookup);  
      
      if (hmm_iter > 0) {
	
	if (postdec_thresh <= 0.0) {

          compute_ehmm_viterbi_alignment(source_sentence[s], cur_lookup, target_sentence[s], 
                                         dict, hmmalign_model[curI-1], initial_prob[curI-1], viterbi_alignment,
                                         hmm_align_mode, false );
        }
	else
	  compute_ehmm_postdec_alignment(source_sentence[s], cur_lookup, target_sentence[s], 
					 dict, hmmalign_model[curI-1], initial_prob[curI-1], hmm_align_mode,
                                         postdec_alignment, postdec_thresh);
      }
      else if (ibm2_iter > 0) {
	
	const Math2D::Matrix<double>& cur_align_model = reduced_ibm2align_model[curI];
	
	if (postdec_thresh <= 0.0) 
	  compute_ibm2_viterbi_alignment(source_sentence[s], cur_lookup, target_sentence[s], dict, 
					 cur_align_model, viterbi_alignment);
	else
	  compute_ibm2_postdec_alignment(source_sentence[s], cur_lookup, target_sentence[s], dict, 
					 cur_align_model, postdec_alignment, postdec_thresh);
	
      }
      else {
	
	if (postdec_thresh <= 0.0) 
	  compute_ibm1_viterbi_alignment(source_sentence[s], cur_lookup, target_sentence[s], dict, viterbi_alignment);
	else
	  compute_ibm1_postdec_alignment(source_sentence[s], cur_lookup, target_sentence[s], dict, 
					 postdec_alignment, postdec_thresh);
      }
      
      if (postdec_thresh <= 0.0) {
	for (uint j=0; j < viterbi_alignment.size(); j++) { 
	  if (viterbi_alignment[j] > 0)
	    (*alignment_stream) << (viterbi_alignment[j]-1) << " " << j << " ";
	}
      }
      else {

	for(std::set<std::pair<AlignBaseType,AlignBaseType> >::iterator it = postdec_alignment.begin(); 
	    it != postdec_alignment.end(); it++) {
	  
	  (*alignment_stream) << (it->second-1) << " " << (it->first-1) << " ";
	}
      }

      (*alignment_stream) << std::endl;
    }

    delete alignment_stream;
    
    if (dev_present) {
      
      std::ostream* dev_alignment_stream;
      
#ifdef HAS_GZSTREAM
      if (string_ends_with(app.getParam("-oa"),".gz")) {
	dev_alignment_stream = new ogzstream(dev_file.c_str());
      }
      else {
#else
      if (true) {
#endif
        dev_alignment_stream = new std::ofstream(dev_file.c_str());
      }
	
	
      for (size_t s = 0; s < dev_source_sentence.size(); s++) {
          
	const uint curI = dev_target_sentence[s].size();

	if (hmm_iter > 0) {
            
	  if (postdec_thresh <= 0.0) {

            compute_ehmm_viterbi_alignment(dev_source_sentence[s],dev_slookup[s], dev_target_sentence[s], 
                                           dict, dev_hmmalign_model[curI-1], dev_initial_prob[curI-1],
                                           viterbi_alignment, hmm_align_mode, false);
	  }
	  else {
	    compute_ehmm_postdec_alignment(dev_source_sentence[s],dev_slookup[s], dev_target_sentence[s], 
					   dict, dev_hmmalign_model[curI-1], dev_initial_prob[curI-1], hmm_align_mode,
					   postdec_alignment, postdec_thresh);
	  }
	}
	else if (ibm2_iter > 0) {
	    
	  if (s == 0) 
	    std::cerr << "Warning: derivation of alignments for IBM-2 is currently only supported on the training set" 
		      << std::endl;
	}
	else {

	  if (postdec_thresh <= 0.0) {            
	    compute_ibm1_viterbi_alignment(dev_source_sentence[s], dev_slookup[s], 
					   dev_target_sentence[s], dict, viterbi_alignment);
	  }
	  else {
	    compute_ibm1_postdec_alignment(dev_source_sentence[s], dev_slookup[s], 
					   dev_target_sentence[s], dict, postdec_alignment, postdec_thresh);
	  }
	}
          
	if (postdec_thresh <= 0.0) {            
	  for (uint j=0; j < viterbi_alignment.size(); j++) { 
	    if (viterbi_alignment[j] > 0)
	      (*dev_alignment_stream) << (viterbi_alignment[j]-1) << " " << j << " ";
	  }
	}
	else {

	  for(std::set<std::pair<AlignBaseType,AlignBaseType> >::iterator it = postdec_alignment.begin(); 
	      it != postdec_alignment.end(); it++) {
	    
	    (*dev_alignment_stream) << (it->second-1) << " " << (it->first-1) << " ";
	  }
	}
	
	(*dev_alignment_stream) << std::endl;
      }
      
      delete dev_alignment_stream;
    }    
  }

  /*** write dictionary ***/
  
  if (app.is_set("-o")) {
    std::ofstream out(app.getParam("-o").c_str());
    for (uint j=0; j < nTargetWords; j++) {
      for (uint k=0; k < dict[j].size(); k++) {
        uint word = (j > 0) ? wcooc[j][k] : k+1;
        if (dict[j][k] > 1e-7)
          out << j << " " << word << " " << dict[j][k] << std::endl;
      }
    }
    out.close();
  }

}
