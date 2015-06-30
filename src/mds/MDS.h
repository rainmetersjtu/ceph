// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */



#ifndef CEPH_MDS_H
#define CEPH_MDS_H

#include "mdstypes.h"

#include "msg/Dispatcher.h"
#include "include/CompatSet.h"
#include "include/types.h"
#include "include/Context.h"
#include "common/DecayCounter.h"
#include "common/perf_counters.h"
#include "common/Mutex.h"
#include "common/Cond.h"
#include "common/Timer.h"
#include "common/LogClient.h"
#include "common/TrackedOp.h"
#include "common/Finisher.h"
#include "common/cmdparse.h"

#include "MDSRank.h"
#include "MDSMap.h"

#include "Beacon.h"


#define CEPH_MDS_PROTOCOL    26 /* cluster internal */

class filepath;

class MonClient;

class Objecter;
class Filer;

class Server;
class Locker;
class MDCache;
class MDBalancer;
class MDSInternalContextBase;

class CInode;
class CDir;
class CDentry;

class Messenger;
class Message;

class MMDSBeacon;

class InoTable;
class SnapServer;
class SnapClient;

class MDSTableServer;
class MDSTableClient;

class AuthAuthorizeHandlerRegistry;

class MDS : public MDSRank, public Dispatcher, public md_config_obs_t {
 public:

  /* Global MDS lock: every time someone takes this, they must
   * also check the `stopping` flag.  If stopping is true, you
   * must either do nothing and immediately drop the lock, or
   * never drop the lock again (i.e. call respawn()) */
  Mutex        mds_lock;
  bool         stopping;

  SafeTimer    timer;

 protected:
  Beacon  beacon;

  AuthAuthorizeHandlerRegistry *authorize_handler_cluster_registry;
  AuthAuthorizeHandlerRegistry *authorize_handler_service_registry;

  std::string name;

  Messenger    *messenger;
  MonClient    *monc;
  MDSMap       *mdsmap;
  LogClient    log_client;
  LogChannelRef clog;
  Finisher finisher;

 public:
  MDS(const std::string &n, Messenger *m, MonClient *mc);
  ~MDS();
  int orig_argc;
  const char **orig_argv;

  // handle a signal (e.g., SIGTERM)
  void handle_signal(int signum);

  // start up, shutdown
  int init(MDSMap::DaemonState wanted_state=MDSMap::STATE_BOOT);

  // config observer bits
  virtual const char** get_tracked_conf_keys() const;
  virtual void handle_conf_change(const struct md_config_t *conf,
				  const std::set <std::string> &changed);
 protected:


  // tick and other timer fun
  class C_MDS_Tick : public MDSInternalContext {
    protected:
      MDS *mds_daemon;
  public:
    C_MDS_Tick(MDS *m) : MDSInternalContext(m), mds_daemon(m) {}
    void finish(int r) {
      mds_daemon->tick_event = 0;
      mds_daemon->tick();
    }
  } *tick_event;
  void     reset_tick();

  // -- client map --
  epoch_t      last_client_mdsmap_bcast;

 private:
  bool ms_dispatch(Message *m);
  bool ms_get_authorizer(int dest_type, AuthAuthorizer **authorizer, bool force_new);
  bool ms_verify_authorizer(Connection *con, int peer_type,
			       int protocol, bufferlist& authorizer_data, bufferlist& authorizer_reply,
			       bool& isvalid, CryptoKey& session_key);
  void ms_handle_accept(Connection *con);
  void ms_handle_connect(Connection *con);
  bool ms_handle_reset(Connection *con);
  void ms_handle_remote_reset(Connection *con);

 protected:
  // admin socket handling
  friend class MDSSocketHook;
  class MDSSocketHook *asok_hook;
  bool asok_command(string command, cmdmap_t& cmdmap, string format,
		    ostream& ss);
  void set_up_admin_socket();
  void clean_up_admin_socket();
  void check_ops_in_flight(); // send off any slow ops to monitor
  void command_scrub_path(Formatter *f, const string& path);
  void command_flush_path(Formatter *f, const string& path);
  void command_flush_journal(Formatter *f);
  void command_get_subtrees(Formatter *f);
  void command_export_dir(Formatter *f,
      const std::string &path, mds_rank_t dest);
  bool command_dirfrag_split(
      cmdmap_t cmdmap,
      std::ostream &ss);
  bool command_dirfrag_merge(
      cmdmap_t cmdmap,
      std::ostream &ss);
  bool command_dirfrag_ls(
      cmdmap_t cmdmap,
      std::ostream &ss,
      Formatter *f);
 private:
  int _command_export_dir(const std::string &path, mds_rank_t dest);
  int _command_flush_journal(std::stringstream *ss);
  CDir *_command_dirfrag_get(
      const cmdmap_t &cmdmap,
      std::ostream &ss);
 protected:
  void create_logger();
  void update_log_config();

  void bcast_mds_map();  // to mounted clients
public:
  /**
   * Terminate this daemon process.
   *
   * This function will return, but once it does so the calling thread
   * must do no more work as all subsystems will have been shut down.
   *
   * @param fast: if true, do not send a message to the mon before shutting
   *              down
   */
  void suicide(bool fast = false);

  /**
   * Start a new daemon process with the same command line parameters that
   * this process was run with, then terminate this process
   */
  void respawn();

  void tick();
  
  // messages
  bool _dispatch(Message *m, bool new_msg);

protected:
  bool handle_core_message(Message *m);
  
  // special message types
  int _handle_command_legacy(std::vector<std::string> args);
  int _handle_command(
      const cmdmap_t &cmdmap,
      bufferlist const &inbl,
      bufferlist *outbl,
      std::string *outs,
      Context **run_later);
  void handle_command(class MMonCommand *m);
  void handle_command(class MCommand *m);
  void handle_mds_map(class MMDSMap *m);
  void _handle_mds_map(MDSMap *oldmap);
};


#endif
