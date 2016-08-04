// bin/latgen-faster-mapped.cc

// Copyright 2009-2012  Microsoft Corporation, Karel Vesely
//                2013  Johns Hopkins University (author: Daniel Povey)
//                2014  Guoguo Chen
//                2016  dgkae
 
// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "tree/context-dep.h"
#include "hmm/transition-model.h"
#include "fstext/fstext-lib.h"
#include "decoder/decoder-wrappers.h"
#include "decoder/decodable-matrix.h"
#include "base/timer.h"

/* for libevent */
#include <event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/*
整理好输入的参数，作为payload_run的参数定义，
把原来的逻辑整理成为payload_init/payload_close/payload_run，
最后与Libevent整合，成为一个常驻内存的以socket作为对外API的服务程序。

注意原来batch的工作模式，还是不变！


关于编译：从原Makefile中剥离出来，使得走原来的编译过程，但是只编译此cmd。
*/

//////////////////////////////////////payload code///////////////////////////////////////
using namespace kaldi;

typedef kaldi::int32 int32;

using fst::SymbolTable;
using fst::VectorFst;
using fst::StdArc;

VectorFst<StdArc> * decode_fst = NULL;
LatticeFasterDecoderConfig config;
TransitionModel trans_model;
BaseFloat acoustic_scale = 0.1;
fst::SymbolTable *word_syms = NULL;
bool determinize = false;
bool allow_partial = false;



void payload_init(int argc, char *argv[])
{
  try {
    KALDI_LOG << "[dgk]payload_init";

    const char *usage =
        "Generate lattices, reading log-likelihoods as matrices\n"
        " (model is needed only for the integer mappings in its transition-model)\n"
        "Usage: latgen-faster-mapped [options] trans-model-in (fst-in|fsts-rspecifier) loglikes-rspecifier"
        " lattice-wspecifier [ words-wspecifier [alignments-wspecifier] ]\n";
    ParseOptions po(usage);

    acoustic_scale = 0.1;
    
    std::string word_syms_filename;
    config.Register(&po);
    po.Register("acoustic-scale", &acoustic_scale, "Scaling factor for acoustic likelihoods");

    po.Register("word-symbol-table", &word_syms_filename, "Symbol table for words [for debug output]");
    po.Register("allow-partial", &allow_partial, "If true, produce output even if end state was not reached.");
    
    po.Read(argc, argv);

    if (po.NumArgs() < 4 || po.NumArgs() > 6) {
      printf("numargs %d", po.NumArgs());
      po.PrintUsage();
      exit(1);
    }

    std::string model_in_filename = po.GetArg(1),
        fst_in_str = po.GetArg(2),
        feature_rspecifier = po.GetArg(3),
        lattice_wspecifier = po.GetArg(4),
        words_wspecifier = po.GetOptArg(5),
        alignment_wspecifier = po.GetOptArg(6);
    
    ReadKaldiObject(model_in_filename, &trans_model);

    determinize = config.determinize_lattice;
    /*
    CompactLatticeWriter compact_lattice_writer;
    LatticeWriter lattice_writer;
    if (! (determinize ? compact_lattice_writer.Open(lattice_wspecifier)
           : lattice_writer.Open(lattice_wspecifier)))
      KALDI_ERR << "Could not open table for writing lattices: "
                 << lattice_wspecifier;*/

    Int32VectorWriter words_writer(words_wspecifier);

    Int32VectorWriter alignment_writer(alignment_wspecifier);

    if (word_syms_filename != "") 
      if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
        KALDI_ERR << "Could not read symbol table from file "
                   << word_syms_filename;

    

    KALDI_LOG << "[dgk] load HCLG";
    Timer loadTimer;
    decode_fst = fst::ReadFstKaldi(fst_in_str);
    double loadElapsed = loadTimer.Elapsed();
    KALDI_LOG << "[dgk] load done, Time taken "<< loadElapsed;



  } catch(const std::exception &e) {
    std::cerr << e.what();
    exit(1);
  }

}

void payload_close(void)
{
  KALDI_LOG << "[dgk]payload_close";

  if (decode_fst != NULL) delete decode_fst; // delete this only after decoder goes out of scope.
  if (word_syms != NULL) delete word_syms;

}

/*简单的参数提取, 以空格分隔*/
std::vector<std::string> dgk_parse_arg(char * seq)
{
  std::vector<std::string> rets;
  
  int i = 0;
  std::string t;
  int s = 0;
  bool out = true;
  while(seq[i] != 0) {
    if (seq[i] != ' ' && out) {
      s = i;
      out = false;
    }

    if (seq[i] == ' ' && !out) {
      seq[i] = 0;
      out = true;
      rets.push_back(std::string(&(seq[s])));
    }

  }
  return rets;
}

int payload_run(char * seq)
{
try {
    using namespace kaldi;

    typedef kaldi::int32 int32;
    using fst::SymbolTable;
    using fst::VectorFst;
    using fst::StdArc;

    double tot_like = 0.0;
    kaldi::int64 frame_count = 0;
    int num_success = 0, num_fail = 0;
    Timer timer;


    std::vector<std::string> args = dgk_parse_arg(seq);
    std::string feature_rspecifier = args[0];
    std::string lattice_wspecifier = args[1];
    std::string words_wspecifier = args[2];
    std::string alignment_wspecifier = args[3];

    CompactLatticeWriter compact_lattice_writer;
    LatticeWriter lattice_writer;
    
    if (! (determinize ? compact_lattice_writer.Open(lattice_wspecifier)
           : lattice_writer.Open(lattice_wspecifier)))
      KALDI_ERR << "Could not open table for writing lattices: "
                 << lattice_wspecifier;
    
    
    Int32VectorWriter words_writer(words_wspecifier);
    Int32VectorWriter alignment_writer(alignment_wspecifier);
    

    {
      SequentialBaseFloatMatrixReader loglike_reader(feature_rspecifier);

      {
        LatticeFasterDecoder decoder(*decode_fst, config);
    
        for (; !loglike_reader.Done(); loglike_reader.Next()) {
          std::string utt = loglike_reader.Key();
          Matrix<BaseFloat> loglikes (loglike_reader.Value());
          loglike_reader.FreeCurrent();
          if (loglikes.NumRows() == 0) {
            KALDI_WARN << "Zero-length utterance: " << utt;
            num_fail++;
            continue;
          }
      
          DecodableMatrixScaledMapped decodable(trans_model, loglikes, acoustic_scale);

          double like;
          if (DecodeUtteranceLatticeFaster(
                  decoder, decodable, trans_model, word_syms, utt,
                  acoustic_scale, determinize, allow_partial, &alignment_writer,
                  &words_writer, &compact_lattice_writer, &lattice_writer,
                  &like)) {
            tot_like += like;
            frame_count += loglikes.NumRows();
            num_success++;
          } else num_fail++;
        }
      }
    } 
      
    double elapsed = timer.Elapsed();
    KALDI_LOG << "Time taken "<< elapsed
              << "s: real-time factor assuming 100 frames/sec is "
              << (elapsed*100.0/frame_count);
    KALDI_LOG << "Done " << num_success << " utterances, failed for "
              << num_fail;
    KALDI_LOG << "Overall log-likelihood per frame is " << (tot_like/frame_count) << " over "
              << frame_count<<" frames.";

    if (num_success != 0) return 0;
    else return 1;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return 2;
  }

}



////////////////////////////////////libevent based server code///////////////////////////////////////////////

#define SERVER_PORT 8080
int debug = 0;

struct client {
  int fd;
  struct bufferevent *buf_ev;
};

int setnonblock(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);

  return 0;
}

void buf_read_callback(struct bufferevent *incoming,
                       void *arg)
{
  struct evbuffer *evreturn;
  char *req;
  int ret = 0;

  req = evbuffer_readline(incoming->input);
  if (req == NULL)
    return;

  
  ret = payload_run(req);/*负载 接受字符串参数，而返回Int*/

  evreturn = evbuffer_new();
  evbuffer_add_printf(evreturn,"ret%d\n",ret);
  bufferevent_write_buffer(incoming,evreturn);
  evbuffer_free(evreturn);
  free(req);
}

void buf_write_callback(struct bufferevent *bev,
                        void *arg)
{
}

void buf_error_callback(struct bufferevent *bev,
                        short what,
                        void *arg)
{
  struct client *client = (struct client *)arg;
  bufferevent_free(client->buf_ev);
  close(client->fd);
  free(client);
}

void accept_callback(int fd,
                     short ev,
                     void *arg)
{
  int client_fd;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  struct client *client;

  client_fd = accept(fd,
                     (struct sockaddr *)&client_addr,
                     &client_len);
  if (client_fd < 0)
    {
      KALDI_WARN << "Client: accept() failed";
      return;
    }

  setnonblock(client_fd);

  client = (struct client *)calloc(1, sizeof(*client));
  if (client == NULL)
    KALDI_ERR << "malloc failed";
  client->fd = client_fd;

  client->buf_ev = bufferevent_new(client_fd,
                                   buf_read_callback,
                                   buf_write_callback,
                                   buf_error_callback,
                                   client);

  bufferevent_enable(client->buf_ev, EV_READ);
}

int main(int argc,
         char **argv)
{
  int socketlisten;
  struct sockaddr_in addresslisten;
  struct event accept_event;
  int reuse = 1;

  event_init();

  socketlisten = socket(AF_INET, SOCK_STREAM, 0);

  if (socketlisten < 0)
    {
      fprintf(stderr,"Failed to create listen socket");
      return 1;
    }

  memset(&addresslisten, 0, sizeof(addresslisten));

  addresslisten.sin_family = AF_INET;
  addresslisten.sin_addr.s_addr = INADDR_ANY;
  addresslisten.sin_port = htons(SERVER_PORT);

  if (bind(socketlisten,
           (struct sockaddr *)&addresslisten,
           sizeof(addresslisten)) < 0)
    {
      fprintf(stderr,"Failed to bind");
      return 1;
    }

  if (listen(socketlisten, 5) < 0)
    {
      fprintf(stderr,"Failed to listen to socket");
      return 1;
    }

  setsockopt(socketlisten,
             SOL_SOCKET,
             SO_REUSEADDR,
             &reuse,
             sizeof(reuse));

  setnonblock(socketlisten);

  event_set(&accept_event,
            socketlisten,
            EV_READ|EV_PERSIST,
            accept_callback,
            NULL);

  event_add(&accept_event,
            NULL);

  
  payload_init(argc, argv);/*负载 初始化*/

  event_dispatch();

  close(socketlisten);

  payload_close();/*负载 释放*/

  return 0;
}