// nnetbin/nnet-forward.cc

// Copyright 2011-2013  Brno University of Technology (Author: Karel Vesely)

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

#include <limits>

#include "nnet/nnet-nnet.h"
#include "nnet/nnet-loss.h"
#include "nnet/nnet-pdf-prior.h"
#include "base/kaldi-common.h"
#include "util/common-utils.h"
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
和latgen-faster-mapped一样，改成daemon形式，
因为每次selecting CUDA和Loading nnet都要1.2秒及0.3秒。
*/

/////////////////////////////////payload code////////////////////////////////////////

using namespace kaldi;
using namespace kaldi::nnet1;
typedef kaldi::int32 int32;

int32 time_shift = 0;
bool apply_log = false;
PdfPriorOptions prior_opts;

Nnet nnet;
Nnet nnet_transf;



void payload_init(int argc, char *argv[])
{
  try {
  
    const char *usage =
      "Perform forward pass through Neural Network.\n"
      "Usage: nnet-forward [options] <nnet1-in> <feature-rspecifier> <feature-wspecifier>\n"
      "e.g.: nnet-forward final.nnet ark:input.ark ark:output.ark\n";

    ParseOptions po(usage);

    prior_opts.Register(&po);

    std::string feature_transform;
    po.Register("feature-transform", &feature_transform,
        "Feature transform in front of main network (in nnet format)");

    bool no_softmax = false;
    po.Register("no-softmax", &no_softmax,
        "Removes the last component with Softmax, if found. The pre-softmax "
        "activations are the output of the network. Decoding them leads to "
        "the same lattices as if we had used 'log-posteriors'.");

    po.Register("apply-log", &apply_log, "Transform NN output by log()");

    std::string use_gpu="no";
    po.Register("use-gpu", &use_gpu,
        "yes|no|optional, only has effect if compiled with CUDA");



    po.Register("time-shift", &time_shift,
        "LSTM : repeat last input frame N-times, discrad N initial output frames.");

    po.Read(argc, argv);

    if (po.NumArgs() != 3) {
      po.PrintUsage();
      exit(1);
    }

    std::string model_filename = po.GetArg(1),
        feature_rspecifier = po.GetArg(2),
        feature_wspecifier = po.GetArg(3);

    // Select the GPU
#if HAVE_CUDA == 1
    CuDevice::Instantiate().SelectGpuId(use_gpu);
#endif

    if (feature_transform != "") {
      nnet_transf.Read(feature_transform);
    }

    
    nnet.Read(model_filename);


    // optionally remove softmax,
    Component::ComponentType last_comp_type = nnet.GetLastComponent().GetType();
    if (no_softmax) {
      if (last_comp_type == Component::kSoftmax ||
          last_comp_type == Component::kBlockSoftmax) {
        KALDI_LOG << "Removing " << Component::TypeToMarker(last_comp_type)
                  << " from the nnet " << model_filename;
        nnet.RemoveLastComponent();
      } else {
        KALDI_WARN << "Last component 'NOT-REMOVED' by --no-softmax=true, "
          << "the component was " << Component::TypeToMarker(last_comp_type);
      }
    }

    // avoid some bad option combinations,
    if (apply_log && no_softmax) {
      KALDI_ERR << "Cannot use both --apply-log=true --no-softmax=true, "
                << "use only one of the two!";
    }

    // we will subtract log-priors later,
    // pdf_prior = PdfPrior(prior_opts);

    // disable dropout,
    nnet_transf.SetDropoutRetention(1.0);
    nnet.SetDropoutRetention(1.0);
    KALDI_LOG << "[dgk] ready!";
  
  
  } catch(const std::exception &e) {
    std::cerr << e.what();
    exit(-1);
  }

}

void payload_close(void)
{

}

/*引号风格的参数提取  "para mmm1" "sdfdf sdfd" 提取的不包含引号*/
std::vector<std::string> dgk_qua_arg(char * seq)
{
  std::vector<std::string> rets;
  int i = 0;
  bool out = true;
  int s = 0;

  while(seq[i] != 0) {
    if (seq[i] == '"' && out) {
      s = i+1;
      out = false;
    } else if (seq[i] == '"' && !out) {
      seq[i] = 0;
      out = true;
      rets.push_back(std::string(&(seq[s])));
    }

    i++;
  }
  return rets;

}

int payload_run(char * seq)
{  
  try {
    kaldi::int64 tot_t = 0;

    std::vector<std::string> args = dgk_qua_arg(seq);

    std::string feature_rspecifier = args[0];
    std::string feature_wspecifier = args[1];

    SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
    BaseFloatMatrixWriter feature_writer(feature_wspecifier);

    CuMatrix<BaseFloat> feats, feats_transf, nnet_out;
    Matrix<BaseFloat> nnet_out_host;
    PdfPrior pdf_prior(prior_opts);

    Timer time;
    double time_now = 0;
    int32 num_done = 0;

    // main loop,
    for (; !feature_reader.Done(); feature_reader.Next()) {
      // read
      Matrix<BaseFloat> mat = feature_reader.Value();
      std::string utt = feature_reader.Key();
      KALDI_VLOG(2) << "Processing utterance " << num_done+1
                    << ", " << utt
                    << ", " << mat.NumRows() << "frm";


      if (!KALDI_ISFINITE(mat.Sum())) {  // check there's no nan/inf,
        KALDI_ERR << "NaN or inf found in features for " << utt;
      }

      // time-shift, copy the last frame of LSTM input N-times,
      if (time_shift > 0) {
        int32 last_row = mat.NumRows() - 1;  // last row,
        mat.Resize(mat.NumRows() + time_shift, mat.NumCols(), kCopyData);
        for (int32 r = last_row+1; r < mat.NumRows(); r++) {
          mat.CopyRowFromVec(mat.Row(last_row), r);  // copy last row,
        }
      }

      // push it to gpu,
      feats = mat;

      // fwd-pass, feature transform,
      nnet_transf.Feedforward(feats, &feats_transf);

      if (!KALDI_ISFINITE(feats_transf.Sum())) {  // check there's no nan/inf,
        KALDI_ERR << "NaN or inf found in transformed-features for " << utt;
      }

      // fwd-pass, nnet,
      nnet.Feedforward(feats_transf, &nnet_out);

      if (!KALDI_ISFINITE(nnet_out.Sum())) {  // check there's no nan/inf,
        KALDI_ERR << "NaN or inf found in nn-output for " << utt;
      }

      // convert posteriors to log-posteriors,
      if (apply_log) {
        if (!(nnet_out.Min() >= 0.0 && nnet_out.Max() <= 1.0)) {
          KALDI_WARN << "Applying 'log()' to data which don't seem to be "
                     << "probabilities," << utt;
        }
        nnet_out.Add(1e-20);  // avoid log(0),
        nnet_out.ApplyLog();
      }

      // subtract log-priors from log-posteriors or pre-softmax,
      if (prior_opts.class_frame_counts != "") {
        pdf_prior.SubtractOnLogpost(&nnet_out);
      }

      // download from GPU,
      nnet_out_host = Matrix<BaseFloat>(nnet_out);

      // time-shift, remove N first frames of LSTM output,
      if (time_shift > 0) {
        Matrix<BaseFloat> tmp(nnet_out_host);
        nnet_out_host = tmp.RowRange(time_shift, tmp.NumRows() - time_shift);
      }

      // write,
      if (!KALDI_ISFINITE(nnet_out_host.Sum())) {  // check there's no nan/inf,
        KALDI_ERR << "NaN or inf found in final output nn-output for " << utt;
      }
      feature_writer.Write(feature_reader.Key(), nnet_out_host);

      // progress log,
      if (num_done % 100 == 0) {
        time_now = time.Elapsed();
        KALDI_VLOG(1) << "After " << num_done << " utterances: time elapsed = "
                      << time_now/60 << " min; processed " << tot_t/time_now
                      << " frames per second.";
      }
      num_done++;
      tot_t += mat.NumRows();


    }

    // final message,
    KALDI_LOG << "Done " << num_done << " files"
              << " in " << time.Elapsed()/60 << "min,"
              << " (fps " << tot_t/time.Elapsed() << ")";

#if HAVE_CUDA == 1
    if (kaldi::g_kaldi_verbose_level >= 1) {
      CuDevice::Instantiate().PrintProfile();
    }
#endif

    if (num_done == 0) return 1;
    return 0;

  } catch(const std::exception &e) {
    std::cerr << e.what();
    return 2;
  }
}



////////////////////////////libevent based server code///////////////////////////////

#define SERVER_PORT 30002
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
  KALDI_LOG << "[dgk] recv " << req;

  
  ret = payload_run(req);/*负载 接受字符串参数，而返回Int*/

  KALDI_LOG << "[dgk] send " << "ret" << ret;

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