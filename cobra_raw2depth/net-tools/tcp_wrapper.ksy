meta:
  id: tcp_wrapper
  endian: be
  imports:
    - bcuda_v1

seq:
  - id: framed_packet
    type: packet
    repeat: expr
    repeat-expr: 100
    # Limit to four packets for sanity in IDE right now...

types:
  packet:
    seq:
      - id: length
        type: u4
      - id: flags
        size: 4
        type: framing_flags
      - id: flag_dependent_1
        size: 4
      - id: flag_dependent_2
        size: 4
      - id: payload
        size: length
        type: bcuda_v1
        
  framing_flags:
    seq:
      - id: padding_only
        type: b1le
      - id: echo_valid
        type: b1le
      - id: echo_new
        type: b1le
      - id: skipstats_present
        type: b1le
      - id: unassigned
        type: b28le
