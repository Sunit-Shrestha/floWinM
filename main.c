#include <X11/X.h>
#include <X11/Xlib.h>
#include <bits/pthreadtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <pthread.h>


#define TOTAL_TAGS 10
#define DEFAULT_MASTER 0.55

Display *display;
Window root;
Screen *scr;

int barheight = 19;
Window barwin;

XEvent ev;
unsigned long border_pixel = 20;
char buffer[1024]; // 1kb buffer for doing stuff later

/// We here define a 2d array
// I hope nobody opens more than 255 windows in a tag, that'd buffer overflow
// the system
//this is the vital security flaw of this window manager

Window clients[255][TOTAL_TAGS] = {0};
unsigned int pertag_win[TOTAL_TAGS] = {0};
float master_size[TOTAL_TAGS] = {
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER, DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    DEFAULT_MASTER,
    };

unsigned int working_tag = 0;
Window focused;
Window preFocused;

void remove_things(Window *array, unsigned int index, unsigned int upto) {
  if (array[index] != 0) {
    printf("there's probably a bug in the remove things function and you know why\n");
  }
  for (int i = index ; i < (upto - 1); i++) {
    array[i] = array[i+1];
  }

}



// the move front and move back functions cause bugs, probably because
// I am bad at using loops properly

void move_front(Window w, unsigned int wor_tag) {
  // making a temporary window because we'll have to do some
  // variable swapping
  Window temp;
  for (int i = 0; i < pertag_win[wor_tag] ; i++) {
    if (clients[wor_tag][i] == w) {
      temp = clients[wor_tag][i];

      // not testig the edge case because it caused some bugs

      if (i == 0) { // swapping first and last
	//	clients[wor_tag][i] = clients[wor_tag][pertag_win[wor_tag] - 1];
	//clients[wor_tag][pertag_win[wor_tag]] = temp;
	printf("begining of the thing, not moving\n");
	return;
      }

      // swapping the windiw and the window after it
      clients[wor_tag][i] = clients[wor_tag][i-1];
      clients[wor_tag][i-1] = temp;
      return;
    }
  }
}


void move_back(Window w, unsigned int wor_tag) {
  // making a temporary window because we'll have to do some
  // variable swapping
  Window temp;
  for (int i = 0; i < pertag_win[wor_tag] ; i++) {
    if (clients[wor_tag][i] == w) {
      temp = clients[wor_tag][i];
      if (i == (pertag_win[wor_tag] - 1)) { // swapping first and last
	//	clients[wor_tag][i] = clients[wor_tag][0];
	//clients[wor_tag][pertag_win[wor_tag]] = temp;
	printf("end of the thing, not moving\n");
	return;
	
      }

      // swapping the window and the window before it
      clients[wor_tag][i] = clients[wor_tag][i+1];
      clients[wor_tag][i+1] = temp;
      return;
    }
  }
}



int remove_window(Window w, unsigned int wor_tag) {
  // search for a window in a tag and remove the window
  for (int i = 0; i < pertag_win[wor_tag] ; i++) {
    if (clients[wor_tag][i] == w) {
      clients[wor_tag][i] = 0;
      remove_things(clients[wor_tag], i, pertag_win[wor_tag]);
      pertag_win[wor_tag] -= 1;
      return 0;
    }
  }
  return 1;
}


int remove_all_instance_of_window(Window w) {
  int retvalue;
  for (int i = 0; i < TOTAL_TAGS ; i++) {
    retvalue = remove_window(w, i);
  }

  return retvalue;
}

void add_window_to_list(Window w) {

  // go to the working tag, go to the last window and add

  for (int i = 0; i < pertag_win[working_tag]; i++) {
    if (clients[working_tag][i] == w) {
      return; // we don't need to anything here now ;
    }
  }
  
  clients[working_tag][pertag_win[working_tag]] = w;

  printf("Adding a window to the tag\n");
  pertag_win[working_tag] += 1; // another window added to the tag

}

void die(char *str) {
  fprintf(stderr, "%s", str);
  exit(0);
}

void unmap_all_tag(unsigned int tag) {
  for (int i = 0 ; i < pertag_win[tag]; i++) {
    XUnmapWindow(display, clients[tag][i]);
  }
}

void unmap_all() {
  for (int i = 0; i < TOTAL_TAGS ; i++) {
    unmap_all_tag(i);
  }
}




void manage(unsigned int working_tag) {
  float master = master_size[working_tag];
  float slave = 1.0 - master;

  printf("the master size is %f\n", master);

  int height = scr->height;
  int width = scr->width;

  height -= barheight;
  unsigned int total_windows_in_this_tag = pertag_win[working_tag];
  unsigned int total_stacks_in_this_tag = total_windows_in_this_tag - 1;


  printf("Total Windows in tag %i : %i\n", working_tag, total_windows_in_this_tag);
  
  for (int i = 0; i < pertag_win[working_tag] ; i++) {
    Window working = clients[working_tag][i]; // get the window to map




    XSetWindowBorder(display, working, border_pixel);

    printf("managing window %lu, INDEX: %i\n", working, i);

    XWindowAttributes atter;
    XGetWindowAttributes(display, working, &atter);
    XMapWindow(display, working);

    
    if (i == 0) { // this means it is the master window
      printf("window %lu is a master\n" , working );

      if (total_windows_in_this_tag == 1) { // take the full size if it is the only window
	XResizeWindow(display, working, width , height);
      } else {
	XResizeWindow(display, working, width * master, height);
      }
      // moving the window to the place of the master
      XMoveWindow(display, working, 0, 0); 
      
    } else { // this means it is the stack window
      printf("window %lu is a slave\n" , working );
      int slave_height = height / total_stacks_in_this_tag;

      printf("HEIGHT: %i, WIDTH: %i\n", slave_height, (int) (width * slave));

      XResizeWindow(display, working, width * slave , slave_height);

      int xpos = width * master;
      int ypos = slave_height * (i - 1) ;

      printf("the position is (%i, %i)\n", xpos, ypos);
      
      XMoveWindow(display, working, width * master, ypos);
    }
  }
}





int wmerror(Display *display, XErrorEvent *ev) {
  die("another wm is already here\n");
  return -1;
}

int error(Display *display, XErrorEvent *ev) {
  char code  = ev->error_code;
  printf("error recived, CODE: %i\n", code);
  return -1;
}

int checkotherwm(Display *display, Window root) {
  XSetErrorHandler(wmerror);
  XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(display, False);
  XSetErrorHandler(error);
  XSync(display, False);
  printf("\nDid not find another wm, Starting....\n");
  return 0;
}

void configureevent(const  XConfigureRequestEvent e) {
  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;

  XConfigureWindow(display, e.window, e.value_mask, &changes);

  manage(working_tag);
}

void mapevent(const XMapRequestEvent e) {
  Window w = e.window;

  printf("Map event came from window %lu\n", w);

  if (e.parent == root) {
    add_window_to_list(w);
  }
  manage(working_tag);
}


void destroynotify(const XDestroyWindowEvent e) {
  Window w = e.window;
  printf("destroy Window %lu\n", w);
  remove_all_instance_of_window(w);
  manage(working_tag);
}


void masterchange(int inc) { // if it is 1, it will increase
  // the user must make sure that this value never reaches below 0 or above 1
  // I am too lazy to code it up right now
  if (inc) {
    master_size[working_tag] += 0.05;
  } else {
    master_size[working_tag] -= 0.05;
  } 
}



void copy_window_to_next_tag(Window w) {

  // get the next tag
  unsigned int next_tag = (working_tag + 1) % TOTAL_TAGS;

  //  add the winodw to the next tag

  for (int i = 0 ; i < pertag_win[next_tag] ; i++) {
    if (w == clients[next_tag][i]) return;
  }

  clients[next_tag][pertag_win[next_tag]] = w;

  // increment the number of windows in the next tag

  pertag_win[next_tag] += 1;
}

void move_window_to_next_tag(Window w) {
  copy_window_to_next_tag(w);
  remove_window(w, working_tag);
}

// I wrote this macro because typing this out everytime while defining a key was
// a fucking chore
// I want this Macro to be used only in the keypress function.
// I can't gaurrentie it will work anywhere outside this function
// Dont bring in bugs

#define ISKEY(K) e.keycode == XKeysymToKeycode(display, XStringToKeysym(K))
#define GOTOTAG(T)  unmap_all(); working_tag = T ; manage(working_tag);


void keypress(const XKeyEvent e) {
  printf("keypresss request seen\n");

  /* if (e.state & Mod1Mask && */

  // shit this code is ugly as fuck
  
  if (e.state & Mod1Mask) {
    if (ISKEY("Q")) {
      Window focused = e.subwindow;
      printf("the focused widow is %lu\n", focused);
      if (focused == barwin) { printf("Cant kill Bar Window\n"); return; }
      XKillClient(display, focused); // Kill the window
      printf("Killed the focuse window %lu\n", focused);
    } else if (ISKEY("D")) {
      printf("opening up dmenu\n");
      system("dmenu_run&");
    } else if (ISKEY("M")) {
      printf("manually running the manage() function\n");
      manage(working_tag);
    } else if (ISKEY("J")) {
      masterchange(0);
      manage(working_tag);
    } else if (ISKEY("K")) {
      masterchange(1);
      manage(working_tag);
    } else if (ISKEY("1")) {
      GOTOTAG(0);
    } else if (ISKEY("2")) {
      GOTOTAG(1);
      printf("Workspace 2 is set");
    } else if (ISKEY("3")) {
      GOTOTAG(2);
    } else if (ISKEY("4")) {
      GOTOTAG(3);
    } else if (ISKEY("5")) {
      GOTOTAG(4);
    } else if (ISKEY("6")) {
      GOTOTAG(5);
    } else if (ISKEY("7")) {
      GOTOTAG(6);
    } else if (ISKEY("8")) {
      GOTOTAG(7);
    } else if (ISKEY("9")) {
      GOTOTAG(8);
    } else if (ISKEY("0")) {
      GOTOTAG(9);
    } else if (ISKEY("H")) {
      move_front(e.subwindow, working_tag);
      manage(working_tag);
    } else if (ISKEY("L")) {
      move_back(e.subwindow, working_tag);
      manage(working_tag);
    } else if (ISKEY("T")) {
      system("st&");
    } else if (ISKEY("C")) {
      exit(0);
    } else if (ISKEY("O")) {
      printf("O key pressed\n");
      if (working_tag == 0) {
	working_tag = TOTAL_TAGS - 1;
      } else {
	working_tag = working_tag - 1;
      }
      GOTOTAG(working_tag);
    } else if (ISKEY("P")) {
      printf("P key pressed\n");
      working_tag = (working_tag + 1) % TOTAL_TAGS;
      GOTOTAG(working_tag);
    } else if (ISKEY("Return")) {
      system("st&");
    } else if (ISKEY("Y")) {
      Window focused = e.subwindow;
      copy_window_to_next_tag(focused);
    } else if (ISKEY("I")) {
      Window focused = e.subwindow;
      move_window_to_next_tag(focused);
      unmap_all();
      manage(working_tag);
    } else if (ISKEY("W")) {
      system("firefox &");
    }
  }
}


/* void update_focused_ev(XFocusChangeEvent f) { */
/*   Window foc = f.window; */
/* } */

int handle_events(XEvent ev) {

  switch (ev.type) {
  case ConfigureRequest:
    configureevent(ev.xconfigurerequest);
    break;
  case MapRequest:
    mapevent(ev.xmaprequest);
    break;
  case DestroyNotify:
    destroynotify(ev.xdestroywindow);
    break;
  case KeyPress:
    keypress(ev.xkey);
    break;
  case FocusIn:
    printf("focusin event just came, 0x%lx gained focus \n\n\n", ev.xfocus.window);
    break;
  case EnterNotify:
    printf("Entered");
    Window entered =  ev.xcrossing.window;
    if (entered != focused) {
      XSetInputFocus(display, entered, RevertToParent, CurrentTime);
    }
    break;
  }

  XSync(display, False);
  return 0;
};


// How these keys work are defined in the function keypress, remember, alt key is the modifier key
#define MOD1BIND(K) XGrabKey(display, XKeysymToKeycode(display, XStringToKeysym(K)), Mod1Mask, root, True, GrabModeAsync, GrabModeAsync)
void setkeys() {
  // for killing windows
  MOD1BIND("Q");
  // for opening up dmenu
  MOD1BIND("D");
  // for running the manage function when there are some bugs
  MOD1BIND("M");
  // dicrease the size of the master
  MOD1BIND("J");
  // increase the size of the master
  MOD1BIND("K");
  // move to tags
  MOD1BIND("1");
  MOD1BIND("2");
  MOD1BIND("3");
  MOD1BIND("4");
  MOD1BIND("5");
  MOD1BIND("6");
  MOD1BIND("7");
  MOD1BIND("8");
  MOD1BIND("9");
  // move wndow front and back
  MOD1BIND("H");
  MOD1BIND("L");
  // for opening up the terminal
  MOD1BIND("T");
  // for exiting the wm
  MOD1BIND("C");

  // add workspace
  MOD1BIND("O");
  // subtract workspace
  MOD1BIND("P");

  // Launch Terminal
  MOD1BIND("Return");
  // Make a copy of the window in the next tag 
  MOD1BIND("Y");
  // Move to the next tag the winodw
  MOD1BIND("I");
  ///
  MOD1BIND("W");
}


Window runbar() {
  Screen screen = *scr;
  Display *dpy = display;
  int win_height = barheight;
  XEvent e;


  Window win = XCreateSimpleWindow(dpy, root, 0, 0, scr->width, win_height, 1,
				   BlackPixel(display, DefaultScreen(display)),
				   WhitePixel(display, DefaultScreen(display)));

  

  XMoveWindow(dpy, win, 0, scr->height - win_height);

  XMapWindow(dpy, win);
  return win;
}



void *update_bar() {
  FILE *fp;
  char tempbu[1024];
  while (1) {
    usleep(100); // sleep for 100 microseconds to let the processor work on other stuff
    fp = fopen("/tmp/sxwm_title", "r");
    if (fp == NULL) {
      continue;  // if the file does not exist, do nothing
    }
    fgets(tempbu, 1000, fp);
    //    printf("temp: %s\nbuf:%s\n", tempbu, buffer );
    if (strcmp(tempbu, buffer)) {
      strcpy(buffer, tempbu);
      XClearWindow(display, barwin);
    }
    XDrawString(display, barwin, DefaultGC(display, DefaultScreen(display)), 10, 10, buffer, strlen(buffer));
    fclose(fp);
  }
}


int main() {

  display = XOpenDisplay(NULL);

  root = DefaultRootWindow(display);

  scr = DefaultScreenOfDisplay(display);

  if (!display) {
    die("The display is not ready\n");
  } else {
    printf("Sucess getting the display\n");
  }

  barwin = runbar();

  pthread_t thread;

  pthread_create(&thread, NULL, update_bar, NULL);
  
  printf("bar window is %lu\n", barwin);

  printf("Gonna check if another window exists");
  
  checkotherwm(display, root);

  printf("setting the keybindings\n");

  setkeys();
  
  printf("Entering the main While loop\n");

  
  while (1) {
    XNextEvent(display, &ev);
    handle_events(ev);
  }

  pthread_join(thread, NULL);
  
  return 0;
} // 469 Nice
