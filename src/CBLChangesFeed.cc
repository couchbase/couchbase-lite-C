//
// CBLChangesFeed.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#include "CBLChangesFeed.h"
#include "Internal.hh"
#include "CBLCheckpoint_Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "ChangesFeed.hh"
#include "DBAccess.hh"
#include "ReplicatorOptions.hh"
#include "c4Replicator.h"

using namespace std;
using namespace litecore::repl;

struct CBLChangesFeed : public CBLRefCounted, public ChangesFeed::Delegate {
    CBLChangesFeed(CBLDatabase* db,
                   CBLChangesFeedOptions feedOptions,
                   CBLCheckpoint* checkpoint,
                   optional<CBLSequenceNumber> since = nullopt)
    :_db(db)
    ,_feedOptions(feedOptions)
    ,_checkpoint(checkpoint)
    ,_since(since)
    {
        if (_checkpoint)
            _options = _checkpoint->options();
        else
            _options.push = kC4Passive;
    }

    void filterToDocIDs(Array docIDs) {
        if (_feed)
            _feed->filterByDocIDs(docIDs);
        else
            _docIDs = docIDs;
    }

    void setFilterFunction(CBLReplicationFilter filter, void *context) {
        Assert(!_feed);
        Assert(filter);
        _filterFunction = filter;
        _filterContext = context;
        _options.callbackContext = this;
        _options.pushFilter = [](C4String docID, C4String revID, C4RevisionFlags flags,
                                 FLDict body, void *context) -> bool {
            return ((CBLChangesFeed*)context)->callFilter(docID, revID, flags, body);
        };
        if (_options.push == kC4OneShot)
            _options.push = kC4Continuous;
    }

    Retained<CBLListenerToken> addListener(CBLChangesFeedListener listener, void *context) {
        Assert(!_feed);
        return _listeners.add(listener, context);
    }

    ChangesFeed& feed() {
        if (!_feed)
            createFeed();
        return *_feed;
    }

protected:
    virtual void dbHasNewChanges() override {
        _db->notify([=]{ _listeners.call(this); });
    }

    virtual void failedToGetChange(ReplicatedRev *rev, C4Error error, bool transient) override {

    }

private:
    void createFeed() {
        _feed.emplace(*this, _options, *_db, _checkpoint.get());
        if (_since != nullopt)
            _feed->setLastSequence(*_since);
        if (_docIDs)
            _feed->filterByDocIDs(move(_docIDs).asArray());
        if (_feedOptions & kCBLChangesFeed_SkipDeletedDocs)
            _feed->setSkipDeletedDocs(true);
        if (_filterFunction)
            _feed->setContinuous(true);
    }

    bool callFilter(slice docID, slice revID, C4RevisionFlags flags, FLDict body) {
        Retained<CBLDocument> doc = new CBLDocument(_db, string(docID), revID, flags, body);
        return _filterFunction(_filterContext, doc, (flags & kRevDeleted) != 0);
    }

    Retained<CBLDatabase> const         _db;
    CBLChangesFeedOptions const         _feedOptions;
    Retained<CBLCheckpoint> const       _checkpoint;
    optional<CBLSequenceNumber> const   _since;
    RetainedValue                       _docIDs;
    CBLReplicationFilter                _filterFunction = nullptr;
    void *                              _filterContext;
    Listeners<CBLChangesFeedListener>   _listeners;
    repl::Options                       _options;
    std::optional<ChangesFeed>          _feed;
};


CBLChangesFeed* CBLChangesFeed_NewWithCheckpoint(CBLDatabase* db,
                                                 CBLCheckpoint* checkpoint,
                                                 CBLChangesFeedOptions options)
{
    return retain(new CBLChangesFeed(db, options, checkpoint));
}


CBLChangesFeed* CBLChangesFeed_NewSince(CBLDatabase* db,
                                        CBLSequenceNumber since,
                                        CBLChangesFeedOptions options)
{
    return retain(new CBLChangesFeed(db, options, nullptr, since));
}


void CBLChangesFeed_FilterToDocIDs(CBLChangesFeed* feed, FLArray docIDs) {
    feed->filterToDocIDs(docIDs);
}


void CBLChangesFeed_SetFilterFunction(CBLChangesFeed* feed,
                                      CBLReplicationFilter filter,
                                      void *context)
{
    feed->setFilterFunction(filter, context);
}


CBLListenerToken* CBLChangesFeed_AddListener(CBLChangesFeed* feed,
                                             CBLChangesFeedListener listener,
                                             void *context)
{
    return retain(feed->addListener(listener, context));
}


CBLSequenceNumber CBLChangesFeed_GetLastSequenceChecked(CBLChangesFeed* feed) {
    return feed->feed().lastSequence();
}


bool CBLChangesFeed_CaughtUp(CBLChangesFeed* feed) {
    return feed->feed().caughtUp();
}


struct CBLChangesFeedRevisionsImpl : public ChangesFeed::Changes,
                                     public CBLChangesFeedRevisions
{
    static void* operator new(size_t size, size_t count) {
        // Account for the full size of the array when allocating heap space:
        return ::operator new(size + (count - 1) * sizeof(CBLChangesFeedRevision));
    }

    CBLChangesFeedRevisionsImpl(ChangesFeed::Changes &&changes)
    :ChangesFeed::Changes(move(changes))
    {
        CBLChangesFeedRevisions::firstSequence = changes.firstSequence;
        CBLChangesFeedRevisions::lastSequence = changes.lastSequence;
        count = revs.size();
        auto *dst = &revisions[0];
        for (const auto &src : revs)
            *dst++ = (const CBLChangesFeedRevision*) &src->docID;
    }
};


CBLChangesFeedRevisions* CBLChangesFeed_Next(CBLChangesFeed* feed, unsigned limit) {
    auto changes = feed->feed().getMoreChanges(limit);
    auto n = changes.revs.size();
    return n ? new (n) CBLChangesFeedRevisionsImpl(move(changes)) : nullptr;
}


void CBLChangesFeedRevisions_Free(CBLChangesFeedRevisions *revs) {
    if (revs)
        delete (CBLChangesFeedRevisionsImpl*)revs;
}
