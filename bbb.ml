open Libav

let _ = Libav.init ();;
let _ = Graphics.open_graph "";;

let get_stream fname =
    let ctx = Libav.open_input fname in
    Libav.set_swscale ctx Libav.RGB24 640 480;
    let get_mtx () = Some (Libav.to_matrix (Libav.get_frame ctx)) in
    Stream.from (fun _ -> try get_mtx () with End_of_file -> None)

let _ =
let frames = get_stream "/home/josh/Desktop/bbbnoaudio.mov" in
try while true do
    let f = Stream.next frames in
    Graphics.draw_image (Graphics.make_image f) 0 0;
    let _ = Graphics.read_key () in ()
done;
with _ -> ();
Graphics.close_graph ()
