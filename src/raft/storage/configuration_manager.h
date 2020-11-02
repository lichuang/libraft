/*
 * Copyright (C) lichuang
 */

#pragma once

namespace libraft {

struct ConfigurationEntry {
    // 表示配置时候的log id，单调递增
    LogId id;
    Configuration conf;
    Configuration old_conf;

    ConfigurationEntry() {}
    ConfigurationEntry(const LogEntry& entry) {
        id = entry.id;
        conf = *(entry.peers);
        if (entry.old_peers) {
            old_conf = *(entry.old_peers);
        }
    }

    bool stable() const { return old_conf.empty(); }
    bool empty() const { return conf.empty(); }
    void list_peers(std::set<PeerId>* peers) {
        peers->clear();
        conf.append_peers(peers);
        old_conf.append_peers(peers);
    }
    bool contains(const PeerId& peer) const
    { return conf.contains(peer) || old_conf.contains(peer); }
};

class ConfigurationManager {

};

};