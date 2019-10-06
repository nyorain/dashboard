- [ ] make daemon with pipe (something like ~/.local/dashboard) that
      allows to activate it (e.g. by just writing *anything* to it,
	  then the window will be mapped. Escape/wm_destroy will just unmap
	  it instead of destroying it. But make sure to reset position and size
	  and stuff and redraw when mapped again)
- [ ] add battery, brightness modules for laptop
	- [ ] battery doesn't have to update while open but brightness should
- [ ] better layout. Instead of hardcoding all positions, define it via
      boxes, margins, paddings and borders i guess?
	- [ ] allow click events? clicking on date/time gives calendar,
	      clicking on notes opens nodes, clicking on music opens
		  ncmpcpp, clicking on notifications opens website etc
		  [probably not worth it, who needs the mouse anyways?]
	- [ ] get more design inspiration on /r/unixporn
	      pretty sure there are some dashboard-like setups already
- [ ] add additional keyboard shortcuts?
- [ ] first-class wayland support via layer shell
- [ ] allow to remove note items from the dashboard (e.g. tab + enter
      or vim j/k or c-n/c-p bindings + enter i guess)
	- [ ] add some way to add notes? [later, not important atm]
	      requires keyboard input support (xkbcommon), probably not worth
		  it for quite a while
		  But when implemented, just add support for general node adding,
		  not only [db] nodes. That could be quite useful...
