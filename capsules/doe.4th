Block 2049
( doe.4th - unified L8 adaptive-map DoE for LithosAnanke              )
( Factors: entropy x cv x temporal_decay x stability, 2 levels each   )
( 2^4 = 16 configurations x 30 reps = 480 randomised runs             )
( Run matrix stores sequential indices 0..479; Fisher-Yates shuffles   )
( them so cfg and rep assignments are preserved through the shuffle.   )
( Decode at runtime: cfg = idx / N-REPS, rep = idx MOD N-REPS         )
( Self-executing: capsule runs DOE on load; CSV streams to serial.     )
49152 CONSTANT ENT-HI    ( 0.75 * 65536 )
 9830 CONSTANT CV-HI     ( 0.15 * 65536 )
32768 CONSTANT TMP-HI    ( 0.50 * 65536 )
32768 CONSTANT STB-HI    ( 0.50 * 65536 )
   16 CONSTANT N-CFG
   30 CONSTANT N-REPS
  480 CONSTANT N-RUNS

Block 2050
( serial output primitives )
: N. ( n -- )
  DUP 0 < IF 45 EMIT ABS THEN
  0 SWAP <# #S #> TYPE ;
: COMMA    44 EMIT ;
: CRLF     13 EMIT 10 EMIT ;
: CSV-COL  ( n -- ) N. COMMA ;
: CSV-LAST ( n -- ) N. CRLF ;
: CSV-HEADER ( -- )
  ." run_id,cfg,rep,ent_in,cv_in,tmp_in,stb_in," CRLF
  ." l8_mode,win_div,infer_win,infer_dec_q," CRLF
  ." infer_var_q,early_exit,bc_mean_q,bb_mean_q,fit_q" CRLF ;

Block 2051
( factor extraction: cfg is 4 bits b3=entropy b2=cv b1=tmp b0=stb )
VARIABLE CURR-CFG
VARIABLE CURR-REP
VARIABLE RUN-ID
: CFG-ENT ( cfg -- q ) 8 AND IF ENT-HI ELSE 0 THEN ;
: CFG-CV  ( cfg -- q ) 4 AND IF CV-HI  ELSE 0 THEN ;
: CFG-TMP ( cfg -- q ) 2 AND IF TMP-HI ELSE 0 THEN ;
: CFG-STB ( cfg -- q ) 1 AND IF STB-HI ELSE 0 THEN ;
: APPLY-CFG ( cfg -- )
  CURR-CFG !
  CURR-CFG @ CFG-ENT
  CURR-CFG @ CFG-CV
  CURR-CFG @ CFG-TMP
  CURR-CFG @ CFG-STB
  L8-UPDATE L8-APPLY ;

Block 2052
( DOE-WORK: arithmetic workload that populates the rolling window     )
( ~35000 word executions per run — enough for heat and window stats   )
: DOE-WORK ( -- )
  PHYSICS-RESET-STATS
  5000 0 DO
    I 13 * 7 +
    I 11 MOD +
    I 3 AND CASE
      0 OF DUP *      ENDOF
      1 OF NEGATE      ENDOF
      2 OF 1 +         ENDOF
      3 OF DROP 0      ENDOF
    ENDCASE
    DROP
  LOOP ;

Block 2053
( run matrix: 480 cells holding sequential indices 0..479             )
( Fisher-Yates shuffles the indices; cfg and rep decoded at runtime   )
( cfg = mat[i] / N-REPS,  rep = mat[i] MOD N-REPS                    )
CREATE RUN-MATRIX N-RUNS 8 * ALLOT
: MATRIX! ( val idx -- ) 8 * RUN-MATRIX + ! ;
: MATRIX@ ( idx     -- val ) 8 * RUN-MATRIX + @ ;
: SWAP-MTX ( i j -- )
  OVER MATRIX@ >R
  OVER MATRIX@ ROT MATRIX!
  R> SWAP MATRIX! ;
: INIT-MATRIX ( -- )
  N-RUNS 0 DO I I MATRIX! LOOP ;

Block 2054
( Fisher-Yates forward shuffle over N-RUNS elements )
( for i in [0, N-RUNS-2]: j = random[i, N-RUNS-1]; swap mat[i] mat[j] )
: SHUFFLE-MATRIX ( -- )
  N-RUNS 1 - 0 DO
    I N-RUNS 1 - RANDOM
    I SWAP-MTX
  LOOP ;

Block 2055
( CSV row emitter )
: EMIT-ROW ( -- )
  INFER-RUN
  RUN-ID @        CSV-COL    ( run_id        )
  CURR-CFG @      CSV-COL    ( cfg           )
  CURR-REP @      CSV-COL    ( rep           )
  CURR-CFG @ CFG-ENT CSV-COL ( ent_in        )
  CURR-CFG @ CFG-CV  CSV-COL ( cv_in         )
  CURR-CFG @ CFG-TMP CSV-COL ( tmp_in        )
  CURR-CFG @ CFG-STB CSV-COL ( stb_in        )
  L8-MODE           CSV-COL  ( l8_mode       )
  WINDOW-DIVERSITY  CSV-COL  ( win_div       )
  INFER-WINDOW@     CSV-COL  ( infer_win     )
  INFER-DECAY@      CSV-COL  ( infer_dec_q   )
  INFER-VARIANCE@   CSV-COL  ( infer_var_q   )
  INFER-EARLY-EXIT@ CSV-COL  ( early_exit    )
  BAYES-CACHE-MEAN  CSV-COL  ( bc_mean_q     )
  BAYES-BUCKET-MEAN CSV-COL  ( bb_mean_q     )
  INFER-FIT@        CSV-LAST ( fit_q         ) ;

Block 2056
( DOE entry point — self-executing on capsule load )
: EXEC-DOE ( seed n-reps -- )
  SWAP SEED
  INIT-MATRIX
  SHUFFLE-MATRIX
  CSV-HEADER
  0 RUN-ID !
  N-CFG * 0 DO
    I MATRIX@
    DUP N-REPS / CURR-CFG !
    N-REPS MOD CURR-REP !
    CURR-CFG @ APPLY-CFG
    DOE-WORK
    EMIT-ROW
    RUN-ID @ 1 + RUN-ID !
  LOOP
  ." DOE: complete" CRLF ;
: DOE ( -- ) 12345 3 EXEC-DOE ;  ( 3 full replicates, seed 12345 )
DOE
