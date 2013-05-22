let _ = Graphics.open_graph "";;

let _ =
let mtx = Libav.get_image "/home/josh/thesis/eva.jpg" in
let img = Graphics.make_image mtx in
Graphics.draw_image img 0 0;
Graphics.read_key ();;
Graphics.close_graph ()
