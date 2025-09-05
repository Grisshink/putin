# Putin

Minimal music player daemon that rivals mpd. (WIP)

## Building

```
make
./putin <music_file_path>
```

## Usage

```
nc -U putin.sock
<type_your_commands_here>
```

## Commands

```
c                   -- Show current status
q                   -- Quit
t                   -- Show raw time and length
s                   -- (Re)start music
s <seconds>         -- Seek to specified position
o <music_file_path> -- Open a different music file
l                   -- Toggle looping
p                   -- Toggle pause
v                   -- Show volume
v <percent>         -- Set volume
pitch               -- Show pitch
pitch <percent>     -- Set pitch
```

## Why putin?

funny

## LICENSE

MIT
