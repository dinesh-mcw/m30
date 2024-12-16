import logging
import time
from pathlib import Path

from flask import Flask, jsonify
from flask_cors import CORS
from flask_restful import Api
from marshmallow import ValidationError
from Pyro5.errors import PyroError

from cobra_system_control.remote import COBRA_ID, COBRA_MESSENGER_ID, remote_lookup

from cobra_lidar_api.api import MSG_Q, configure_api


log = logging.getLogger("cobra_system_control.cobra_log")
log.addHandler(logging.NullHandler())

# Create the web app
root = Path(__file__).parent
static = Path(root, "m30_webapp").absolute()

SUCCESS_MSG = "System Bootup Complete"
NO_SENSOR_MSG = "No sensor head detected"
MESSAGE_TIMEOUT_S = 2
TRIES_TO_TIMEOUT = 4


def get_update_msg():
    """Attempts to get the next update message in the message queue
    if the Cobra Messenger object is up but the Cobra object is not.
    If there are no messages, waits up to two seconds for a message.
    If there are no messages after two seconds, returns an empty
    string. If the Cobra object exists, the function immediately
    returns "System Bootup Complete." or "No sensor head detected."
    if no sensor is connected to the NCB.

    This is written as a separate method to enable endpoint mocking.
    """
    try:
        with remote_lookup(COBRA_ID, log_warnings=False) as cb:
            if not cb.sen:
                return NO_SENSOR_MSG
            else:
                return SUCCESS_MSG
    except PyroError:
        tries = 0
        while tries < TRIES_TO_TIMEOUT:
            try:
                with remote_lookup(COBRA_MESSENGER_ID, log_warnings=False) as cm:
                    msg = cm.get(block=True, timeout=MESSAGE_TIMEOUT_S)
                    return msg;
            except PyroError:
                tries += 1
                time.sleep(0.5)
            except Queue.Empty:
                return ""

        return (
            "Waiting for system startup. If you see this "
            "message for more than five minutes, please power cycle the sensor. "
            "If this message continues to persist, contact customer support."
        )



class M30Api(Api):
    """Custom overrides of base ``flask-restful`` behavior.
    """

    def handle_error(self, e):
        log.exception("Handling a raised error", exc_info=e)
        if isinstance(e, ValidationError):
            # For some reason ``abort`` doesn't work???
            # This is how the response should be though, so...
            return e.messages, 422
        else:
            # ``super().handle_error(e)`` doesn't work either... wtf...
            raise e


def create_app():
    app = Flask(
        __name__,
        root_path=str(root),
        static_folder=str(static),
        static_url_path="/",
    )
    app.config["JSON_SORT_KEYS"] = False
    CORS(app)

    api = M30Api(app)
    configure_api(api)

    @app.route("/")
    @app.route("/index")
    def index():
        return app.send_static_file("index.html")

    @app.route('/messages/update')
    def msg():
        return get_update_msg()

    @app.route("/messages/")
    @app.route("/messages")
    def status():
        # Simple way - drain the queue and return array of contents. Empty default. Needs polling.
        ret = []
        while not MSG_Q.empty():
            ret.append(MSG_Q.get())
        return jsonify(ret)

    MSG_Q.put("API Configured.")
    return app


def main():
    app = create_app()
    app.run(host="0.0.0.0", port=5001)


if __name__ == '__main__':
    main()
