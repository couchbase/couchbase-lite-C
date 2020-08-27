from ._PyCBL import ffi, lib
from .common import *


class ReplicatorConfiguration:
    def __init__(self, database, url, push_filter):
        self.database = database
        self.endpoint = lib.CBLEndpoint_NewWithURL(cstr(url))
        self.type = 0
        self.continuous = True
        self.authenticator = ffi.NULL
        self.proxy_settings = ffi.NULL
        self.headers = ffi.NULL
        self.pinned_server_cert = []
        self.truested_root_cert = []
        self.channels = ffi.NULL
        self.document_ids = ffi.NULL
        self.push_filter = ffi.NULL
        self.pull_filter = ffi.NULL
        self.conflict_resolver = ffi.NULL
        self.context = ffi.NULL

    def _cblConfig(self):
        return ffi.new("CBLReplicatorConfiguration*",
                       [self.database._ref,
                        self.endpoint,
                        self.type,
                        self.continuous,
                        self.authenticator,
                        self.proxy_settings,
                        self.headers,
                        self.pinned_server_cert,
                        self.truested_root_cert,
                        self.channels,
                        self.document_ids,
                        self.push_filter,
                        self.pull_filter,
                        self.conflict_resolver,
                        self.context])


class Replicator (CBLObject):
    def __init__(self, config):
        if config != None:
            config = config._cblConfig()
        CBLObject.__init__(self,
                           lib.CBLReplicator_New(config, gError),
                           "Couldn't create replicator", gError)
        self.config = config

    def start(self):
        lib.CBLReplicator_Start(self._ref)

    def stop(self):
        lib.CBLReplicator_Stop(self._ref)