- [x] correctly display when volume is muted
- [x] make daemon with pipe (something like ~/.local/dashboard) that
      allows to activate it (e.g. by just writing *anything* to it,
	  then the window will be mapped. Escape/wm_destroy will just unmap
	  it instead of destroying it. But make sure to reset position and size
	  and stuff and redraw when mapped again)
- [x] add battery module for laptop
	- [x] battery doesn't have to update while open
- [ ] refresh displayed time in dashboard. Probably best done via timerfd/
      timeout tracking in main loop?
	- [ ] just refreshing it every 60 seconds isn't enough.
	      should be more procise. Proably possible with time/date
		  functions from c/posix though
- [ ] better layout. Instead of hardcoding all positions, define it via
      boxes, margins, paddings and borders i guess?
	- [ ] allow click events? clicking on date/time gives calendar,
	      clicking on notes opens nodes, clicking on music opens
		  ncmpcpp, clicking on notifications opens website etc
		  [probably not worth it, who needs the mouse anyways?]
	- [ ] get more design inspiration on /r/unixporn
	      pretty sure there are some dashboard-like setups already
- first-class native wayland support via layer shell
	- [x] factor display code out into seperate file
	- [x] we can probably re-use some utility from swaybg (pool-buffer)
	- [ ] find out why banner surface doesn't work with wlr
	      pretty sure it's a bug in wlr/sway though, maybe just report?
		  if not fixed/can't find it: workaround by simply destroying
		  the layer surface every time the window is hidden
	- [ ] later: include output management
	- [ ] support keyboard input, foward to ui
- [x] allow to remove note items from the dashboard (e.g. tab + enter
      or vim j/k or c-n/c-p bindings + enter i guess)
	- [ ] add some way to add notes? [later, not important atm]
	      requires keyboard input support (xkbcommon), probably not worth
		  it for quite a while
		  But when implemented, just add support for general node adding,
		  not only [db] nodes. That could be quite useful...
- [ ] add additional keyboard shortcuts?
- [x] mpd notification: if the text doesn't fit into the
      notification window, use "..."
	- [ ] better mpd notification layout? with the title displaying
	      in a larger font and the artist in a second line?
- [x] make modules compile-time options.
      shouldn't require to have sqlite/alsa/mpd installed
- [ ] go all the way with renaming dashboard ->
      dui (desktop/dashboard ui/utility information; some mashup of those)
- [ ] volume module: show current output/allow switching
      maybe allow to name them manually (e.g. headphone/speaker/monitor).
	  We probably need to use pulseaudio instead of alsa for that though.
	  And that requires us to use their sophisticated (speak: complicated)
	  main loop approach.
	  See https://github.com/pulseaudio/pulseaudio/blob/master/src/utils/pactl.c
	- [ ] current bug: when audio output is switched, volume change
	      isn't noticed correctly anymore.
		  Could maybe be fixed without using pulseaudio though
	- [ ] best solution probably: pulse audio backend
- more modules
	- [ ] number of github notifications
	      just show it in dashboard for now (async curl request to github api),
		  later on we automatically check with a timeout.
		  But if a new notification comes, leave the notification display
		  to notify-send (and the corresponding server)
	- [ ] mails?
	- [ ] show items from daily/weekly notes?
	- [ ] something about calendar/more reminders?
	- [ ] something about weather?
	- [ ] some random modules showing suggestions/random stuff?
- [ ] in dummy implementations: assert that they are not used?
      currently the functions are just no-ops but if they are ever called,
	  it's clearly an error. Maybe assert(false && ...)?

Not sure if useful for this project or seperate project:
there probably already is something for this i guess.
But we monitor changes for those values anyways.
That goes beyond the dashboard functionality; rather something
like "desktop-ui"

- [x] small window when changing volume/brightness
	- [x] show the currently playing song when it is changed
	      *manually*. Not sure if mpd has a signal for that though...
		  i guess this is only possible if this application takes
		  over controlling mpd next/prev. Do that via the command pipe
	- [ ] also show notification when battery is getting low (10% and 5%)
	      that one is hard/ugly. procfs for battery doesn't support
		  inotify (which kinda makes sense). We would have to monitor
		  the power state with a fixed interval (e.g. every 100s)
		  for a simple inequality check (like < 10%) this should
		  be good enough

later/not sure yet:
- [x] replace mpd control with playerctl? (or add it as alternative)
      could show the first player we find that is currently playing.
	  if none is playing, show mpd (note that this requires mpd-mpris)
	  in case e.g. chromium is playing, could also show
	  little chromium sign
	  [probably not worth it for now]
