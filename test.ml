let _ = Graphics.open_graph "";;

let _ =
let frame = Libav.get_image "/home/josh/thesis/eva.jpg" in
let img = Graphics.make_image (Libav.to_matrix frame) in
Libav.write_image frame "asdf.png";
Graphics.draw_image img 0 0;
Graphics.read_key ();;
Graphics.close_graph ()
