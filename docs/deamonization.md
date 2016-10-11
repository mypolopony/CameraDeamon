## Deamonization

<small>Nota bene: here, and throughout, the word daemon is spelled incorrectly. This began innocently but is now pervasive.</small>  

When the serving PC is turned on, two services are automatically started:
- The first, `server_logic`, controls the EmbeddedServer -- it necessarily denies the default network-manager from beginning to ensure that the machine will not join any wifi networks. It then starts and maintains an internal LAN, _agridatahome_, which makes available the HTTP server.
- The second, `cameradeamon`, logically controls the CameraDeamon  

These two processes are controlled in the canonical way via _systemctl_ and are set to respawn in the case that either are disrupted. Do note, though, that if something has gone wrong, _systemctl_ will only try five or so times before giving up entirely