//
// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef FIREBASE_ARDUINO_H
#define FIREBASE_ARDUINO_H

#include <string>

#include "Firebase.h"
#include "FirebaseObject.h"

/**
 * Main class for Arduino clients to interact with Firebase.
 * This implementation is designed to follow Arduino best practices and favor
 * simplicity over all else.
 * For more complicated usecases and more control see the Firebase class in
 * Firebase.h.
 */
class FirebaseArduino {
 public:
  /**
   * Must be called first. This initialize the client with the given
   * firebase host and credentials.
   * \param host Your firebase db host, usually X.firebaseio.com.
   * \param auth Optional credentials for the db, a secret or token.
   */
  virtual void begin(const String& host,const String& auth = "");
  
  virtual void updateAuth(const String& auth);

  

  virtual void stream(const String& path);
  virtual void endStream();

  /**
   * Checks if there are new events available. This is only meaningful once
   * stream() has been called.
   * \return If a new event is ready.
   */
  virtual bool available();

  /**
   * Reads the next event in a stream. This is only meaningful once stream() has
   * been called.
   * \return FirebaseObject will have ["type"] that describes the event type, ["path"]
   * that describes the effected path and ["data"] that was updated.
   */
  virtual FirebaseObject readEvent();

  /**
   * \return Whether the last command was successful.
   */
  bool success();

  /**
   * \return Whether the last command failed.
   */
  bool failed();

  /**
   * \return Error message from last command if failed() is true.
   */
  virtual const String& error();
 private:
  std::string host_;
  std::string auth_;
  FirebaseError error_;
  std::shared_ptr<FirebaseHttpClient> req_http_;
  std::shared_ptr<FirebaseRequest> req_;
  std::shared_ptr<FirebaseHttpClient> stream_http_;
  std::shared_ptr<FirebaseStream> stream_;

  void initStream();
  void initRequest();
  void getRequest(const String& path);
};

extern FirebaseArduino Firebase;

#endif // FIREBASE_ARDUINO_H
