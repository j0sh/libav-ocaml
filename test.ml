let _ = Graphics.open_graph "";;

let shuffle = Array.sort (fun _ _ -> (Random.int 3) - 1)

let _ =
let frame = Libav.get_image "/home/josh/thesis/eva.jpg" in
let mtx = Libav.to_matrix frame in
let _ = shuffle mtx in
let newframe = Libav.from_matrix mtx in
Libav.write_image newframe "asdf.png";
Graphics.draw_image (Graphics.make_image mtx) 0 0;
Graphics.read_key ();;
Graphics.close_graph ()
