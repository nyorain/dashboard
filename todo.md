- [x] correctly display when volume is muted
- [ ] replace mpd control with playerctl? (or add it as alternative)
      could show the first player we find that is currently playing.
	  if none is playing, show mpd (note that this requires mpd-mpris)
	  in case e.g. chromium is playing, could also show
	  little chromium sign
- [ ] make daemon with pipe (something like ~/.local/dashboard) that
      allows to activate it (e.g. by just writing *anything* to it,
	  then the window will be mapped. Escape/wm_destroy will just unmap
	  it instead of destroying it. But make sure to reset position and size
	  and stuff and redraw when mapped again)
- [x] add battery module for laptop
	- [x] battery doesn't have to update while open
- [ ] better layout. Instead of hardcoding all positions, define it via
      boxes, margins, paddings and borders i guess?
	- [ ] allow click events? clicking on date/time gives calendar,
	      clicking on notes opens nodes, clicking on music opens
		  ncmpcpp, clicking on notifications opens website etc
		  [probably not worth it, who needs the mouse anyways?]
	- [ ] get more design inspiration on /r/unixporn
	      pretty sure there are some dashboard-like setups already
- [ ] add additional keyboard shortcuts?
- [ ] first-class native wayland support via layer shell
	- [ ] factor display code out into seperate file
- [ ] allow to remove note items from the dashboard (e.g. tab + enter
      or vim j/k or c-n/c-p bindings + enter i guess)
	- [ ] add some way to add notes? [later, not important atm]
	      requires keyboard input support (xkbcommon), probably not worth
		  it for quite a while
		  But when implemented, just add support for general node adding,
		  not only [db] nodes. That could be quite useful...

Not sure if useful for this project or seperate project:
there probably already is something for this i guess.
But we monitor changes for those values anyways.
That goes beyond the dashboard functionality; rather something
like "desktop-ui"

- [ ] small window when changing volume/brightness/song
