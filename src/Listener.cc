//
// Listener.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "Listener.hh"
#include "CBLDatabase.h"

using namespace std;


void CBLListenerToken::remove() {
    auto oldOwner = _owner;
    if (oldOwner) {
        _callback = nullptr;
        _owner = nullptr;
        oldOwner->remove(this);
    }
}


NotificationQueue::NotificationQueue(CBLDatabase *database _cbl_nonnull)
:_database(database)
,_state(State())
{ }

void NotificationQueue::setCallback(CBLNotificationsReadyCallback callback, void *context) {
    auto pending = _state.use<Notifications>([&](State &state) {
        state.callback = callback;
        state.context = context;
        return callback ? nullptr : move(state.queue);
    });
    call(pending);
}


void NotificationQueue::add(Notification notification) {
    bool notifyNow = false;
    CBLNotificationsReadyCallback readyCallback = nullptr;
    void* readyContext;

    _state.use([&](State &state) {
        if (state.callback) {
            bool first = !state.queue;
            if (first)
                state.queue.reset( new vector<Notification> );
            state.queue->push_back(notification);
            if (first) {
                readyCallback = state.callback;
                readyContext = state.context;
            }
        } else {
            notifyNow = true;
        }
    });

    if (notifyNow)
        notification();                         // immediate notification
    else if (readyCallback)
        readyCallback(readyContext, _database); // notify that notifications are queued
}


void NotificationQueue::notifyAll() {
    auto queue = _state.use<Notifications>([&](State &state) {
        return move(state.queue);
    });
    call(queue);
}


void NotificationQueue::call(const Notifications &queue) {
    if (queue) {
        for (Notification &n : *queue)
            n();
    }
}
