import requests
import json

TELEGRAM_TOKEN = "8675453870:AAGYUxeiaWRnOXiMfT3eyarvxDCFI1zmK4Y"
CHAT_ID = "-5171515546'"

print("Listening for fall alerts on ntfy.sh...")

while True:
    try:
        resp = requests.get("https://ntfy.sh/cg2028-fall-alert/json", stream=True, timeout=90)
        for line in resp.iter_lines():
            if line:
                data = json.loads(line)
                if data.get("event") == "message":
                    msg = data.get("message", "Fall alert!")
                    title = data.get("title", "Alert")
                    text = f"[{title}] {msg}"
                    requests.post(
                        f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage",
                        json={"chat_id": CHAT_ID, "text": text}
                    )
                    print(f"Forwarded to Telegram: {text}")
    except requests.exceptions.Timeout:
        continue
    except Exception as e:
        print(f"Connection lost, reconnecting... ({e})")