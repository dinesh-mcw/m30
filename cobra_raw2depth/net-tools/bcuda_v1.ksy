meta:
  id: bcuda_v1
  endian: be

seq:
  - id: global_header
    type: global_header
  - id: type_header
    type: type_headers
  - id: payload
    type: payloads

types:
  global_header:
    seq:
    - id: magic
      contents: [0x42, 0x43, 0x44, 0x41]
    - id: protocol_version
      type: b4
    - id: type
      type: b4
    - id: device_version
      type: u4
    - id: sequence_num
      type: u4
    - id: device_id
      type: u4
    - id: reserved
      type: u4
      
  type_headers:
    seq:
      - id: type_specific_header
        type:
          switch-on: _parent.global_header.type
          cases:
            0x2: type_2_header
            0xC: type_c_header
            0xD: type_d_header
            
  payloads:
    seq:
      - id: type_specific_payload
        type: 
          switch-on: _parent.global_header.type
          cases:
            0x2: type_2_payload
            0xC: type_c_payload
            0xD: type_d_payload
            
  type_2_header:
    seq:
      - id: timestamp
        size: 10
      - id: timescale
        type: b4
        # Enum me
      - id: advisory_sequence_flags
        type: b4
        # Expand me
      - id: advisory_last_scene_start_seq
        type: u4
      - id: advisory_last_scene_end_seq
        type: u4
      - id: advisory_current_scene_start_seq
        type: u4
      - id: advisory_current_scene_end_seq
        type: u4
      - id: complete_size_steer_dim
        type: u2
      - id: complete_size_stare_dim
        type: u2
      - id: payload_steer_offset
        type: u2
      - id: payload_stare_offset
        type: u2
      - id: reserved0
        type: u2
      - id: reserved1
        type: u4
  
  type_2_payload:
    seq:
      - id: returns
        type: type_2_return
        repeat: expr
        repeat-expr: 64
  
  type_2_return:  
    seq:
      - id: intensity
        type: u2
      - id: range
        type: u2
      - id: background
        type: u2
      - id: snr
        type: u2
      - id: extra_annotation
        type: u1
      - id: range_present_valid
        type: b1le
      - id: intensity_present_valid
        type: b1le
      - id: background_present_valid
        type: b1le
      - id: snr_present_valid
        type: b1le
      - id: extra_type
        type: b4le
        # Enum me

  type_d_header:
    seq:
      - id: timestamp
        size: 10
      - id: timescale
        type: b4
        # Expand me
      - id: advisory_sequence_flags
        type: b4
        # Expand me
      - id: advisory_last_scene_start_seq
        type: u4
      - id: advisory_last_scene_end_seq
        type: u4
      - id: advisory_current_scene_start_seq
        type: u4
      - id: advisory_current_scene_end_seq
        type: u4
      - id: complete_size_steer_dim
        type: u2
      - id: complete_size_stare_dim
        type: u2
      - id: payload_steer_offset
        type: u2
      - id: payload_stare_offset
        type: u2
      - id: bs_steer_offset
        type: u2
      - id: bs_steer_step
        type: u2
      # Note that reserved0, reserved1 were consumed (for bs_stare_offset and bs_stare_step), and the
      # "change" became reserved2. Later, reserved2 was completely consumed for bs_user_tag. 
      - id: bs_stare_offset
        type: u2
      - id: bs_stare_step
        type: u2
      - id: bs_user_tag
        type: u2

  type_d_payload:
    seq:
      - id: returns
        type: type_d_return
        repeat: expr
        repeat-expr: 64
  
  type_d_return:  
    seq:
      - id: intensity
        type: u2
      - id: range
        type: u2
      - id: background
        type: u2
      - id: snr
        type: u2
      - id: extra_annotation
        type: u1
      - id: range_present_valid
        type: b1le
      - id: intensity_present_valid
        type: b1le
      - id: background_present_valid
        type: b1le
      - id: snr_present_valid
        type: b1le
      - id: extra_type
        type: b4le
        # Enum me

  type_c_header:
    seq:
      - id: image_end_u
        type: u2
      - id: image_end_v
        type: u2
      - id: payload_start_u
        type: u2
      - id: payload_start_v
        type: u2
      - id: parameter_type
        type: u1
        enum: type_c_parameter_type
      - id: reserved0
        type: u1
      - id: reserved1
        type: u2
      - id: reserved2
        type: u4

  type_c_payload:
    seq:
      - id: parameters
        type: type_c_payload_cm32t32p
        repeat: expr
        repeat-expr: 64
  
  type_c_payload_cm32t32p:
    seq:
      - id: theta
        type: s4
      - id: phi
        type: s4

enums:
  type_c_parameter_type:
    1: none
    2: coordinate_map_32as_theta_32as_phi
