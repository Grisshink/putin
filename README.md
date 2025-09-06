# Putin

Minimal music player daemon that rivals mpd. (WIP)

## Building

```
make
make install
```

## Usage

```
nc -U putin.sock
<type_your_commands_here>
```

## Commands

```
status                 -- Show current status
quit                   -- Quit
time                   -- Show raw time and length
seek                   -- (Re)start music
seek <seconds>         -- Seek to specified position
play <music_file_path> -- Open a different music file
loop                   -- Toggle looping
pause                  -- Toggle pause
volume                 -- Show volume
volume <percent>       -- Set volume
pitch                  -- Show pitch
pitch <percent>        -- Set pitch
```

## Why putin?

funny

## LICENSE

MIT
