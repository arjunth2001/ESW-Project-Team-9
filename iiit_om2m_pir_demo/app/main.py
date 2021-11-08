from onem2mlib import *
import onem2mlib.notifications as NOT
import onem2mlib.constants as CON
import hashlib
import sqlite3
import atexit, threading, json
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse
import onem2mlib
import onem2mlib.exceptions as EXC
import onem2mlib.constants as CON
import onem2mlib.internal as INT
import requests
import uuid

import matplotlib.pylab as plt
import extra_streamlit_components as stx
import pandas as pd
import streamlit as st

PAGE_CONFIG = {'page_title': 'ESW Project PIR Demo IIIT OM2M', 'layout': "wide"}
st.set_page_config(**PAGE_CONFIG)

_server = None
_thread = None
_port = 1400

def get_data():
    headers = {
        'X-M2M-Origin': 'rTI9uxe@NN:7rn5sOIaYf',
        'Content-type': 'application/json'
     }

    response = requests.get("http://esw-onem2m.iiit.ac.in:443/~/in-cse/in-name/Team-9/pir?rcn=4", headers=headers)
    _resp = json.loads(response.text)
    i = [int(i["con"]) for i in _resp['m2m:cnt']['m2m:cin']]
    print(i)
    return i

def call_back(res):
    global session_id
    print(resource.content)
    rerun(session_id=session_id)

# Start the notification server in a background threa
def _startNotificationServer():
	global _server, _thread
	if _thread:
		return
	# Start processing requests in a separate thread.
	# Listen on any interface/IP address.
	_server = HTTPNotificationServer(('', _port), HTTPNotificationHandler)
	_thread = threading.Thread(target=_server.run)
	_thread.start()

# Stop the thread/notification server
@atexit.register
def _stopNotificationServer():
	global _server, _thread
	if not _server or not _thread:
		return
	# Shutdown server
	_server.shutdown()
	_thread.join()
	_server = None
	_thread = None

# This class implements the notification server that runs in the background.
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
			#print(post_data)
			threading.Thread(target=self._handleJSON(post_data), args=(post_data)).start()
			

	# Catch and ignore all log messages
	def log_message(self, format, *args):
		return
	

	# Handle JSON notifications 
	def _handleJSON(self, data):
		jsn =  json.loads(data.decode('utf-8'))
		#print(jsn)
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
    if 'x' not in st.session_state:
        try:
            _startNotificationServer()
        except:
            pass
    st.session_state.y = get_data()
    st.session_state.x = list(range(len(st.session_state.y)))

    st.title("ESW Project Team 9: PIR Demo with OM2M Subscription")
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
            with c2:
                st.markdown("### Occupancy")
                fig, ax = plt.subplots()
                fig.patch.set_facecolor('#EAE7DC')
                ax = plt.plot(st.session_state.x, st.session_state.y)
                st.pyplot(fig)
            with c3:
                pass


if __name__ == '__main__':
    main()
