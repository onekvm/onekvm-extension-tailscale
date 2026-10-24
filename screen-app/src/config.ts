import type { AppConfig } from "@onekvm/screen-ui";
import { actionEnabled, actionLabel } from "./status";

export const CONFIG: AppConfig = {
  actionEnabled,
  actionLabel,
  "extension": "tailscale",
  "title": "Tailscale",
  "icon": "src/logo.png",
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
