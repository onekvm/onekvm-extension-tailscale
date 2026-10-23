import type { AppConfig } from "./types";

export const CONFIG: AppConfig = {
  "extension": "tailscale",
  "title": "Tailscale",
  "glyph": "T",
  "statusMethod": "status",
  "kind": "network",
  "actions": [
    {
      "method": "connect",
      "title": "Connect"
    },
    {
      "method": "disconnect",
      "title": "Disconnect"
    }
  ]
};
