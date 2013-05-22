open Graphics

let _ =
Graphics.open_graph "";;
Graphics.set_color magenta;;
Graphics.draw_circle 100 100 100;;
Graphics.clear_graph;;
read_key ();;
close_graph ()
