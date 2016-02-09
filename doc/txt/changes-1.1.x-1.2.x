- Due to changes in the FSA you MUST run $AFD_WORK_DIR/sbin/convert_fsa
  and $AFD_WORK_DIR/sbin/convert_jid before starting Version 1.2.x if
  you have been running an older AFD (1.1.x).

- Complete rewrite of ftp proxy procedure. The login procedure for
  proxies is now specified with the $U<login-name> and $P<password>
  in any order and as many times as required. For example if you
  have to login to the proxy with the username anonymous and the
  password user@host and then login using the user and password
  of that specified in the DIR_CONFIG, you would need to do the
  following: $Uanonymous;$Puser@host;$U;$P

  With this the option <secure ftp> and <pproxy> will fall away
  and are no longer supported.

- Removed option <no login>. This situation should now hopefully
  be detected automatically.

- In the DIR_CONFIG configuration file there is a new optional
  identifier [dir options] which comes just after the directory
  entry. Here you can now specify what is to be done with files
  for which there is no distribution rule and lots of other new
  options (see documentation for more details). The old format
  of specifying directory options is still supported, but
  support for it will soon be dropped.

- The parameters MAX_NO_DEST_GROUP and MAX_NO_RECIPIENT in config.h
  have fallen away. There is now no more limit for the number of
  destination groups and the number of recipients per recipient
  group. MAX_NO_FILES has also fallen away, instead MAX_FILE_MASK_BUFFER
  is now used. This is one fixed buffer length and under the [files]
  option, you may now specify as many files as fit into this buffer.

- The number of process that AMG may spawn for a certain directory
  can now be limited with MAX_PROCESS_PER_DIR (default is 10).
