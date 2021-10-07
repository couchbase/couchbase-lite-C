//
// CBLLog_Internal.hh
//
// Copyright © 2021 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/**
 * Initialize the log level of each log domains to DEBUG so that
 * the log level of all domains could be controlled by using c4log_setCallbackLevel()
 * and c4log_writeToBinaryFile() and setup the log callback with the WARNING as its default
 * log level.
 *
 * This method is safe to call multiple times but the initializing logic will be executed only
 * once when the method is called the first time.
 *
 * @NOTE As we cannot use static initializer to initialize the log level and the log callback,
 * we will need to call CBLLog_Init() from the top level methods that logs (level < Warnings)
 * are expected including CBLDatabase's open(), copyDatabase(), deleteDatabase(),
 * Andriod's CBL_Init(), and any methods used for configuring logs in CBLLog.cc.
 *
 * In the future, if calling CBLLog_Init() becomes painfully hard to track, we could consider
 * changing the default log level of each domain in LiteCore from INFO to WARNING so that calling
 * CBLLog_Init() is not needed beside from the methods used for configuring logs.
 */
void CBLLog_Init();
