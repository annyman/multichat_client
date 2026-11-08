# multichat_client

Terminal client for `multichat_server`.

## Run

```sh
python3 client.py
```

Prompts for the server IP and username, then opens a TUI with scrollable chat history and an input bar at the bottom. The server port is always `8080`. Use `PgUp`/`PgDn` or the arrow keys to scroll, and send `close` to quit. The username acts as the password; reusing it restores the same client id and color.
