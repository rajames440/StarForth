Block 4055
( common:msg.4th — Hermes participant interface )
( Load into every messaging VM at birth: S" common:msg.4th" USE )
: HERMES-ACK ( -- )
  S" MSG-ACK-LAST" S" Hermes" VM-EXEC ;
: HERMES-NACK ( -- )
  S" MSG-NACK-LAST" S" Hermes" VM-EXEC ;
