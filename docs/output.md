## Output

- System log file to `/var/logs/agridata/agridata.log`
- Frame log file to `/home/agridata/output`
- Video file to `/home/agridata/output`

## Analysis

This will likely be bulked up at a later time, but currently under development (as part of [Quality Assurance](https://github.com/motioniq/QualityAssurance)), is a suite to process and analyze videos, frame-by-frame. Check out the repository for more up to date information.

## Frame Loss

Frame loss, as has been described elsewhere, is a natural component of even a well-defined software architecture. There are two schools of thought here: the first attempts to obtain 20 frames per second at all times and to minimize frame loss and time variation between frames. The second opinion is that it is of no real value to chase a very stable rate of 20fps and that any analysis can proceed assuming a _variable frame rate_, which is sufficieny. Also, even if there were some way to accurately obtain 20fps, the speed of the ATV introduces more variation (in terms of time-spent-looking-at-fruit).  

These two positions are not necessarily mutually exclusive but further work should be done to understand which approach is more valuable.