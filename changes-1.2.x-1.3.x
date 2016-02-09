


         PLEASE READ THIS BEFORE UPDATING to 1.3.x !


There are numerous changes in Version 1.3.x that make it incompatible
to earlier Versions. However AFD tries to detect this automatically and
do the conversion from 1.2.x to 1.3.x without any interaction. This has
not been well tested so if you do not want to take any risk perform the
following steps:

   1) Stop AMG and wait until FD has distributed all files. If FD cannot
      transmit the files because the remote host is for example not
      reachable, you either have to wait or loose those files.

   2) Stop AFD with the following command:

         afd -s

   3) Install the new AFD.

   4) Next you will need to initialize AFD. There are two methods, a
      complete initialization where all internal data of AFD is deleted,
      also the log files. This type of initialiazation is done as follows:

         afd -I

      If you like to keep the log files and statistics do the initialization
      with the following command:

         afd -i

      But notice the show_olog, show_ilog and show_dlog dialogs will not
      be able to display the old log files. But as time goes by these old
      log files are removed.

   5) Now you may start AFD again:

         afd -a

Note that the format of HOST_CONFIG file has changed, this will be
automatically changed by AFD. The format of INPUT_LOG, OUTPUT_LOG and
DELETE_LOG has also changed considerably. There is no tool that does
the conversion from the old to the new version of those log files. Sorry!

See Changelog for what has been changed.
