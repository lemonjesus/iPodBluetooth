require "rubyserial"

class IPod
  attr_reader :serialport

  def initialize(device)
    @serialport = Serial.new device, 19200
  end

  def send_command(mode, command, parameter = [])
    header = [0xff, 0x55]
    length = [1 + command.size + parameter.size]
    checksum = [0x100 - ((length + [mode] + command + parameter).sum & 0xff)]
    packet = (header + length + [mode] + command + parameter + checksum).pack("C*")
    @serialport.write packet
    packet
  end

  def mode_switch(mode)
    send_command(0x00, [0x01, mode])
  end

  # mode 0x02 functions (basic control)
  {
    release_button: [0x00, 0x00],
    play_pause: [0x00, 0x01],
    volume_up: [0x00, 0x02],
    volume_down: [0x00, 0x04],
    skip_forward: [0x00, 0x08],
    skip_backward: [0x00, 0x10],
    next_album: [0x00, 0x20],
    previous_album: [0x00, 0x40],
    stop: [0x00, 0x80],
    play: [0x00, 0x00, 0x01],
    pause: [0x00, 0x00, 0x02],
    toggle_mute: [0x00, 0x00, 0x04],
    next_playlist: [0x00, 0x00, 0x20],
    previous_playlist: [0x00, 0x00, 0x40],
    toggle_shuffle: [0x00, 0x00, 0x80],
    toggle_repeat: [0x00, 0x00, 0x00, 0x01],
    ipod_off: [0x00, 0x00, 0x00, 0x04],
    ipod_on: [0x00, 0x00, 0x00, 0x08],
    press_menu: [0x00, 0x00, 0x00, 0x40],
    press_select: [0x00, 0x00, 0x00, 0x80],
    scroll_up: [0x00, 0x00, 0x00, 0x00, 0x01],
    scroll_down: [0x00, 0x00, 0x00, 0x00, 0x02]
  }.each do |name, command|
    define_method name do
      send_command(0x02, command)
      sleep 0.05
      send_command(0x02, [0x00, 0x00])
      nil
    end
  end  
end

ipod = IPod.new("/dev/ttyUSB0")
