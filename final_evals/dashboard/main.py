from onem2mlib import *
import onem2mlib.notifications as NOT
import onem2mlib.constants as CON
import hashlib
import sqlite3
import atexit
import threading
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse
import onem2mlib
import onem2mlib.exceptions as EXC
import onem2mlib.constants as CON
import onem2mlib.internal as INT
import requests
import uuid
import matplotlib
import matplotlib.pylab as plt
import extra_streamlit_components as stx
import pandas as pd
import streamlit as st
import numpy as np
from matplotlib.colors import ListedColormap
from dateutil import parser

# Encryption
import re
import base64
from base64 import b64decode
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
import hmac
import hashlib
PAGE_CONFIG = {
    'page_title': 'ESW Project', 'layout': "wide"}
st.set_page_config(**PAGE_CONFIG)
IP = "10.11.0.158"
CONTAINER = "testing"


def get_session_id():
    ctx = st.report_thread.get_report_ctx()
    session_id = ctx.session_id

    return session_id


def rerun(session_id=None):
    if session_id is None:
        session_id = get_session_id()

    server = st.server.server.Server.get_current()
    session = server._get_session_info(  # pylint: disable = protected-access
        session_id
    ).session

    session.request_rerun(None)


session_id = get_session_id()


def hash_hmac256(message):
    API_SECRET = "ESWTEAM9"

    signature = hmac.new(
        bytes(API_SECRET, 'ascii'),
        msg=bytes(message, 'ascii'),
        digestmod=hashlib.sha256
    ).hexdigest()
    return signature


def decode_base64(data, altchars=b'+/'):
    data = re.sub(rb'[^a-zA-Z0-9%s]+' % altchars, b'', data)
    missing_padding = len(data) % 4
    if missing_padding:
        data += b'=' * (4 - missing_padding)
    return base64.b64decode(data, altchars)


def decode_AES(ciphertext):
    key = "2B7E151628AED2A6ABF7158809CF4F3C"
    iv = "AAAAAAAAAAAAAAAAAAAAAA=="

    key = bytearray.fromhex(key)
    iv = b64decode(iv).hex()
    iv = bytearray.fromhex(iv)
    ciphertext = b64decode(ciphertext)

    cipher = AES.new(key, AES.MODE_CBC, iv)
    decrypted = cipher.decrypt(ciphertext)
    decryptedUnpad = unpad(decrypted, AES.block_size)
    return decode_base64(decryptedUnpad).decode("utf-8")


def verify_hash(to_decode, to_hash):
    to_decode = decode_AES(to_decode)
    calc_hash = hash_hmac256(to_decode)
    print("-----------")
    print(to_decode, to_hash)
    print(calc_hash)
    print("---------------")
    if calc_hash == to_hash:
        print("Hash verified. Successful transfer.")
        return True
    else:
        print("Hash doesn't match. Something fishy.")
        return True


def sigmoid(x):
    return 1 / (1 + np.exp(-x))


def eval_LR(x):
    x = x.astype(np.float)
    with open("./weights_lr.json") as f:
        d = json.load(f)
    p_w = d["w"]
    p_i = d["i"]
    ans = sigmoid(p_w@x + p_i)
    if ans > 0.5:
        return 1
    else:
        return 0


def parse(content):
    _id, f1, f2, f3, a, time = content.split(",")
    time = parser.parse(time)
    a = list(a)
    p = eval_LR(np.array([f1, f2, f3]))
    a = [int(i) for i in a]
    a = np.array(a)
    print(_id)
    print(f1)
    print(f2)
    print(f3)
    print(time)
    print(p)
    a = np.reshape(a, (8, 8))
    print("a")
    print(a)
    return a, p, time


if "_server" not in st.session_state:
    st.session_state._server = None
    st.session_state._thread = None
    st.session_state._port = 1400


def get_data():
    headers = {
        'X-M2M-Origin': 'rTI9uxe@NN:7rn5sOIaYf',
        'Content-type': 'application/json'
    }

    response = requests.get(
        f"http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-9/{CONTAINER}?rcn=4", headers=headers)
    _resp = json.loads(response.text)
    try:
        res = [i["con"]for i in _resp['m2m:cnt']['m2m:cin']]
    except Exception as e:
        print(e)
        res = []

    print(res)
    ret = []
    for i in res:
        tup = i.split(',')
        to_decode, to_hash = tup[0], tup[1]
        if verify_hash(to_decode, to_hash):
            dec_content = decode_AES(to_decode)
            ret.append(parse(dec_content))
    return ret


def get_latest_a():
    headers = {
        'X-M2M-Origin': 'rTI9uxe@NN:7rn5sOIaYf',
        'Content-type': 'application/json'
    }

    response = requests.get(
        f"http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-9/{CONTAINER}/la", headers=headers)
    _resp = json.loads(response.text)
    i = _resp["m2m:cin"]["con"]
    tup = i.split(',')
    to_decode, to_hash = tup[0], tup[1]
    if verify_hash(to_decode, to_hash):
        dec_content = decode_AES(to_decode)
        return parse(dec_content)
    else:
        return np.zeros((8, 8)), 0, 0


def call_back(resource):
    global session_id
    print("call_back;", resource.content)
    rerun(session_id=session_id)

# Start the notification server in a background threa


def _startNotificationServer():
    if st.session_state._thread:
        return
    # Start processing requests in a separate thread.
    # Listen on any interface/IP address.
    st.session_state._server = HTTPNotificationServer(
        ('', st.session_state._port), HTTPNotificationHandler)
    st.session_state._thread = threading.Thread(
        target=st.session_state._server.run)
    st.session_state._thread.start()

# Stop the thread/notification server


@ atexit.register
def _stopNotificationServer():
    if not st.session_state._server or not st.session_state._thread:
        return
    # Shutdown server
    st.session_state._server.shutdown()
    st.session_state._thread.join()
    st.session_state._server = None
    st.session_state._thread = None

# This class implements the notification server that runs in the background.


@ st.cache
def subscribe():
    headers = {
        'X-M2M-Origin': 'rTI9uxe@NN:7rn5sOIaYf',
        'Content-type': 'application/json;ty=23'
    }

    body = {
        "m2m:sub": {
            "rn": str(uuid.uuid4()),
            "nu": [f"http://{IP}:{st.session_state._port}/"],
            "nct": 1
        }
    }
    try:
        response = requests.post(
            f"http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-9/{CONTAINER}/", json=body, headers=headers)
    except TypeError:
        response = requests.post(
            f"http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-9/{CONTAINER}/", data=json.dumps(body), headers=headers)
    print('Return code : {}'.format(response.status_code))
    print('Return Content : {}'.format(response.text))
    return


class HTTPNotificationServer(HTTPServer):
    def run(self):
        try:
            self.serve_forever()
        finally:
            # Clean-up server (close socket, etc.)
            self.server_close()

# This class implements the handler that reseives the requests


class HTTPNotificationHandler(BaseHTTPRequestHandler):
    # Handle incoming notifications (POST requests)
    def do_POST(self):
        # Construct return header
        self.send_response(200)
        self.send_header('X-M2M-RSC', '2000')
        self.end_headers()

        # Get headers and content data
        length = int(self.headers['Content-Length'])
        contentType = self.headers['Content-Type']
        post_data = self.rfile.read(length)
        # print(post_data)
        threading.Thread(target=self._handleJSON(
            post_data), args=(post_data)).start()

    # Catch and ignore all log messages

    def log_message(self, format, *args):
        return

    # Handle JSON notifications

    def _handleJSON(self, data):
        jsn = json.loads(data.decode('utf-8'))
        # print(jsn)
        # check verification request
        vrq = INT.getALLSubElementsJSON(jsn, 'vrq')
        if len(vrq) == 0:
            vrq = INT.getALLSubElementsJSON(jsn, 'm2m:vrq')
        if len(vrq) > 0 and vrq[0] == True:
            return 	# do nothing

        # get the sur first
        sur = INT.getALLSubElementsJSON(jsn, 'sur')
        if len(sur) == 0:
            sur = INT.getALLSubElementsJSON(jsn, 'm2m:sur')
        if len(sur) > 0:
            sur = sur[0]
        else:
            return 	# must have a subscription ID

        # get resource
        rep = INT.getALLSubElementsJSON(jsn, 'rep')
        if len(rep) == 0:
            rep = INT.getALLSubElementsJSON(jsn, 'm2m:rep')
        if len(rep) > 0:
            jsn = rep[0]
            type = INT.getALLSubElementsJSON(jsn, 'ty')
            if type and len(type) > 0:
                resource = INT._newResourceFromType(type[0], None)
                resource._parseJSON(jsn)
                call_back(resource)
###############################################################################


def make_hashes(password):
    return hashlib.sha256(str.encode(password)).hexdigest()


def check_hashes(password, hashed_text):
    if make_hashes(password) == hashed_text:
        return hashed_text
    return False


conn = sqlite3.connect('data.db')
c = conn.cursor()


def create_usertable():
    c.execute('CREATE TABLE IF NOT EXISTS userstable(username TEXT,password TEXT)')


def add_userdata(username, password):
    c.execute('INSERT INTO userstable(username,password) VALUES (?,?)',
              (username, password))
    conn.commit()


def login_user(username, password):
    c.execute('SELECT * FROM userstable WHERE username =? AND password = ?',
              (username, password))
    data = c.fetchall()
    return data


def view_all_users():
    c.execute('SELECT * FROM userstable')
    data = c.fetchall()
    return data


def auth_state():
    if 'user' not in st.session_state:
        st.session_state.user = None
    return st.session_state


def main():
    if '_thread' not in st.session_state or st.session_state._thread == None:
        try:
            _startNotificationServer()
            subscribe()
        except:
            pass
    if 'x' not in st.session_state:
        a, p, t = get_latest_a()
        st.session_state.grid = a
        st.session_state.x = [t]
        st.session_state.y = [p]
    else:
        a, p, t = get_latest_a()
        st.session_state.grid = a
        st.session_state.x.append(t)
        st.session_state.y.append(p)
        if len(st.session_state.x) >= 4:
            st.session_state.x = st.session_state.x[1:]
            st.session_state.y = st.session_state.y[1:]

    st.title("ESW Project Team 9")
    if auth_state().user == None:
        st.subheader("Login")
        st.write(
            "You can login to the dashboard here. Contact admin to get access. Thank you.")
        username = st.text_input("Username")
        password = st.text_input("Password", type='password')
        if st.button("Login"):
            create_usertable()
            hashed_pswd = make_hashes(password)
            result = login_user(username, check_hashes(password, hashed_pswd))
            if result:
                auth_state().user = username
                st.experimental_rerun()
            else:
                st.warning("Incorrect Username/Password")
    else:
        st.success(f"Logged in as {auth_state().user}")
        if st.button("Logout"):
            auth_state().user = None
            st.experimental_rerun()

        st.subheader("Dashboard")
        choices1 = [
            stx.TabBarItemData(id=1, title="Dashboard",
                               description="Dashboard for Visualisations"),
        ]
        if auth_state().user == "admin":
            choices = choices1 + \
                [stx.TabBarItemData(
                    id=len(choices1)+1, title="Admin Panel", description="Panel for Admin Tasks")]
        else:
            choices = choices1
        chosen_id = stx.tab_bar(data=choices, default=1)
        if int(chosen_id) == len(choices1)+1:
            task = st.selectbox("Task", ["Add a new User", "View all Users"])
            if task == "Add a new User":
                st.subheader("Add a new User")
                new_user = st.text_input("Username")
                new_password = st.text_input("Password", type='password')

                if st.button("Add"):
                    create_usertable()
                    add_userdata(new_user, make_hashes(new_password))
                    st.success("You have successfully created a valid Account")
                    st.info("This user can now access the app")

            elif task == "View all Users":
                st.subheader("User Profiles")
                user_result = view_all_users()
                clean_db = pd.DataFrame(user_result, columns=[
                                        "Username", "Password"])
                st.dataframe(clean_db)
        else:
            c1, c2, c3 = st.columns((1, 1, 1))
            with c1:
                pass
                st.markdown("### Grid")
                cmapmine = ListedColormap(['white', 'black'], N=2)
                fig, ax = plt.subplots()
                fig.patch.set_facecolor('#EAE7DC')
                ax.imshow(st.session_state.grid,
                          cmap=cmapmine, vmin=0, vmax=1)
                ax.set_title('Zeros and Ones')
                st.pyplot(fig)

            with c2:
                st.markdown("### Occupancy")
                fig, ax = plt.subplots()
                fig.patch.set_facecolor('#EAE7DC')
                dates = matplotlib.dates.date2num(st.session_state.x)
                ax = plt.plot(dates, st.session_state.y)
                st.pyplot(fig)

            with c3:
                pass


if __name__ == '__main__':
    main()
