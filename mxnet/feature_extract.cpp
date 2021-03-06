/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 */
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "mxnet-cpp/MxNetCpp.h"

#include "SgDebug.h"
#include "SgTimer.h"
#include "SgSystem.h"
#include "SgRandom.h"
#include "GoUctPureRandomGenerator.h"

#include "GoInit.h"
#include "SgInit.h"
#include "GoBoard.h"
#include "GoUctBoard.h"

#include <boost/utility.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>



using namespace std;
// using namespace SgTime;
using namespace mxnet::cpp;

/*
 * This example shows how to extract features with a pretrained model.
 * Get the model here:
 *   https://github.com/dmlc/mxnet-model-gallery
 * */

/*The global context, change them if necessary*/
// Context global_ctx(kGPU, 0);
Context global_ctx(kCPU,0);

class FeatureExtractor {
 private:
  /*the mean image, get from the pretrained model*/
  NDArray mean_img;
  /*the following two maps store all the paramters need by the model*/
  map<string, NDArray> args_map;
  map<string, NDArray> aux_map;
  Symbol net;
  Executor *executor;
  /*Get the feature layer we want to extract*/
  void GetFeatureSymbol() {
    /*
     * use the following to check all the layers' names:
     * */
    /*
    net=Symbol::Load("./model/Inception_BN-symbol.json").GetInternals();
    for(const auto & layer_name:net.ListOutputs()){
      LG<<layer_name;
    }
    */
    // net = Symbol::Load("./model/Inception-BN-symbol.json")
    //           .GetInternals()["global_pool_output"];

    // net = Symbol::Load("./model/zero_resnet-symbol.json")
    //           .GetInternals()["softmax_output"];

    net = Symbol::Load("./model/zero_super_simple_cnn-symbol.json")
              .GetInternals()["softmax_output"];
              
  }
  /*Fill the trained paramters into the model, a.k.a. net, executor*/
  void LoadParameters() {
    map<string, NDArray> paramters;
    // NDArray::Load("./model/Inception-BN-0126.params", 0, &paramters);
    // NDArray::Load("./model/zero_resnet-0003.params", 0, &paramters);
    NDArray::Load("./model/zero_super_simple_cnn-0010.params", 0, &paramters);

        


    for (const auto &k : paramters) {
      if (k.first.substr(0, 4) == "aux:") {
        auto name = k.first.substr(4, k.first.size() - 4);
        aux_map[name] = k.second.Copy(global_ctx);
      }
      if (k.first.substr(0, 4) == "arg:") {
        auto name = k.first.substr(4, k.first.size() - 4);
        args_map[name] = k.second.Copy(global_ctx);
      }
    }
    /*WaitAll is need when we copy data between GPU and the main memory*/
    NDArray::WaitAll();
  }
  void GetMeanImg() {
    mean_img = NDArray(Shape(1, 3, 224, 224), global_ctx, false);
    mean_img.SyncCopyFromCPU(
        NDArray::LoadToMap("./model/mean_224.nd")["mean_img"].GetData(),
        1 * 3 * 224 * 224);
    NDArray::WaitAll();
  }

 public:
  FeatureExtractor() {
    /*prepare the model, fill the pretrained parameters, get the mean image*/
    SgDebug() << "Starting to get feature symbol. \n";

    GetFeatureSymbol();

    SgDebug() << "Starting to load parameters. \n";

    LoadParameters();

    SgDebug() << "Starting to get image mean. \n";

    GetMeanImg();

    SgDebug() << "Feature Extractor loaded. \n";
  }

  void Extract(NDArray data) {
    /*Normalize the pictures*/
    // data.Slice(0, 1) -= mean_img;
    // data.Slice(1, 2) -= mean_img;
    
    args_map["data"] = data;
    /*bind the excutor*/

    SgDebug() << "Trying to bind the model. \n";

    NDArray array;

    SgTimer timer;

    double start_time;
    start_time = timer.GetTime();

    for (int i=0; i< 100; i++){
      executor = net.SimpleBind(global_ctx, args_map, map<string, NDArray>(),
                              map<string, OpReqType>(), aux_map);

      // SgDebug() << "After binding. \n";

      
      executor->Forward(false);
      /*print out the features*/
      array = executor->outputs[0].Copy(Context(kCPU, 0));
      NDArray::WaitAll();

    }

    

    double end_time;
    end_time = timer.GetTime();



    float maxValue = 0;
    float curValue = 0;
    int maxPosition = 0;

    for (int i = 0; i < 362; ++i) {
      curValue = array.At(0, i);

      if (curValue > maxValue) {
        maxValue = curValue;
        maxPosition = i;
      }
      cout << array.At(0, i) << ",";
    }
    cout << endl;

    cout << "MaxValue: " << maxValue << endl;
    cout << "MaxPosition: " << maxPosition << endl;

    cout << "time used: " << (end_time - start_time) << endl;

  }
};

NDArray Data2NDArray() {
  NDArray ret(Shape(2, 3, 224, 224), global_ctx, false);
  ifstream inf("./img.dat", ios::binary);
  vector<float> data(2 * 3 * 224 * 224);
  inf.read(reinterpret_cast<char *>(data.data()), 2 * 3 * 224 * 224 * sizeof(float));
  inf.close();
  ret.SyncCopyFromCPU(data.data(), 2 * 3 * 224 * 224);
  NDArray::WaitAll();
  return ret;
}

NDArray generateSampleData(){
  NDArray ret(Shape(1, 17, 19, 19), global_ctx, false);
  
  vector<float> data(1 * 17 * 19 * 19);
  // inf.read(reinterpret_cast<char *>(data.data()), 2 * 3 * 224 * 224 * sizeof(float));
  // inf.close();
  
  for (int i=0; i < 19; i++){
    for (int j=0; j < 19; j++){
      data[16*(i+1)*(j+1)] = 1;
      // ret[0][16][i][j] = 1;
    }
  }
  

  ret.SyncCopyFromCPU(data.data(), 1 * 17 * 19 * 19);
  NDArray::WaitAll();
  return ret;
}

void goBoardTest(){

  SgInit();
  GoInit();

  SgPoint testingPoint;

  GoBoard basicBoard;
  GoUctBoard goBoard(basicBoard);

  SgRandom sgRandom;

  GoUctPureRandomGenerator<GoUctBoard> randomGenerator(goBoard, sgRandom);
  randomGenerator.Start();

  for (int i =0; i< 1000; i++){
      // testingPoint = curPolicy.GenerateMove();
      testingPoint = randomGenerator.Generate();

      if (testingPoint == SG_NULLMOVE){
          break;
      }

      // SgDebug() << "testingPoint " << i << ": " << testingPoint << " . \n";

      // SgPoint testingPoint = SgPointUtil::Pt(10, 10);
      // SgBlackWhite color = SG_BLACK;

      goBoard.Play(testingPoint);

  }

  SgDebug() << "Finished testing the GoUctBoard. \n";

}

int main() {
  /*
   * get the data from a binary file ./img.data
   * this file is generated by ./prepare_data_with_opencv
   * it stores 2 pictures in NDArray format
   *
   */

  goBoardTest();

  auto data = generateSampleData();
  FeatureExtractor fe;
  fe.Extract(data);
  return 0;
}
