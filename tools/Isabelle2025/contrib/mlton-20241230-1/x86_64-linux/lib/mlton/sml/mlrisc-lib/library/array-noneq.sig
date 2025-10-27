signature ARRAY_NONEQ =
   sig
      type 'a array
      type 'a vector

      val all: ('a -> bool) -> 'a array -> bool
      val app: ('a -> unit) -> 'a array -> unit
      val appi: (int * 'a -> unit) -> 'a array -> unit
      val array: int * 'a -> 'a array
      val collate: ('a * 'a -> order) -> 'a array * 'a array -> order
      val copy: {src: 'a array, dst: 'a array, di: int} -> unit
      val copyVec: {src: 'a vector, dst: 'a array, di: int} -> unit
      val exists: ('a -> bool) -> 'a array -> bool
      val find: ('a -> bool) -> 'a array -> 'a option
      val findi: (int * 'a -> bool) -> 'a array -> (int * 'a) option
      val foldl: ('a * 'b -> 'b) -> 'b -> 'a array -> 'b
      val foldli: (int * 'a * 'b -> 'b) -> 'b -> 'a array -> 'b
      val foldr: ('a * 'b -> 'b) -> 'b -> 'a array -> 'b
      val foldri: (int * 'a * 'b -> 'b) -> 'b -> 'a array -> 'b
      val fromList: 'a list -> 'a array
      val length: 'a array -> int
      val maxLen: int
      val modify: ('a -> 'a) -> 'a array -> unit
      val modifyi: (int * 'a -> 'a) -> 'a array -> unit
      val sub: 'a array * int -> 'a
      val tabulate: int * (int -> 'a) -> 'a array
      val update: 'a array * int * 'a -> unit
      val vector: 'a array -> 'a vector

(*
      val toList: 'a array -> 'a list
      val fromVector: 'a vector -> 'a array
      val toVector: 'a array -> 'a vector
*)
   end
(*
functor Chk(A : ARRAY) : ARRAY_NONEQ = A
functor Chk(A : ARRAY_NONEQ
                where type 'a array = 'a array
                where type 'a vector = 'a vector) : ARRAY = A
*)
