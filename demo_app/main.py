import streamlit as st
import pandas as pd
import extra_streamlit_components as stx
import seaborn as sns
import matplotlib.pylab as plt
import numpy as np
import time
import glob, random
import random
from streamlit.report_thread import get_report_ctx
from streamlit.server.server import Server
from threading import Thread
from time import sleep
import streamlit as st
from streamlit_autorefresh import st_autorefresh

# Run the autorefresh about every 1000 milliseconds (1 seconds) and stop
# after it's been refreshed 100 times.
PAGE_CONFIG = {'page_title':'ESW Project'}
st.set_page_config(**PAGE_CONFIG)

import hashlib
count = st_autorefresh(interval=3000, limit=None, key="counter")
def make_hashes(password):
    return hashlib.sha256(str.encode(password)).hexdigest()

def check_hashes(password,hashed_text):
    if make_hashes(password) == hashed_text:
        return hashed_text
    return False

import sqlite3
conn = sqlite3.connect('data.db')
c = conn.cursor()

def create_usertable():
    c.execute('CREATE TABLE IF NOT EXISTS userstable(username TEXT,password TEXT)')

def add_userdata(username,password):
    c.execute('INSERT INTO userstable(username,password) VALUES (?,?)',(username,password))
    conn.commit()

def login_user(username,password):
    c.execute('SELECT * FROM userstable WHERE username =? AND password = ?',(username,password))
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
    if "reload" not in st.session_state:
        st.session_state.reload=True
        st.session_state.count=0
        st.session_state.occupants=[1,0,3,4]
        st.session_state.hour = [1,2,3,4]
    else:
        st.session_state.count+=1
        st.warning(f"rerun:{st.session_state.count}")
        st.session_state.occupants.append(np.random.choice([0,1,2,4,5]))
        st.session_state.hour.append((st.session_state.hour[-1]+1)%24)
        if len(st.session_state.occupants)>=24:
            st.session_state.occupants = st.session_state.occupants[1:]
            st.session_state.hour = st.session_state.hour[1:]

    file_path_type = ["./images/*.png", "./images/*.jpeg","./images/*.jpg"]
    images = glob.glob(random.choice(file_path_type))
    random_image = random.choice(images)
    st.session_state.image= random_image
    st.session_state.grid_eye = np.random.rand(8,8)
    st.title("ESW Project Team 9")
    if auth_state().user == None:
        st.subheader("Login")
        st.write("You can login to the dashboard here. Contact admin to get access. Thank you.")
        username = st.text_input("Username")
        password = st.text_input("Password",type='password')
        if st.button("Login"):
            create_usertable()
            hashed_pswd = make_hashes(password)
            result = login_user(username,check_hashes(password,hashed_pswd))
            if result:
                auth_state().user=username
                st.experimental_rerun()
            else:
                st.warning("Incorrect Username/Password")
    else:
        st.success(f"Logged in as {auth_state().user}")
        if st.button("Logout"):
            auth_state().user=None
            st.experimental_rerun()

        st.subheader("Dashboard")
        choices1 =[
            stx.TabBarItemData(id=1, title="Dashboard", description="Dashboard for Visualisations"),
        ]
        if auth_state().user=="admin":
            choices = choices1 + [stx.TabBarItemData(id=len(choices1)+1, title="Admin Panel", description="Panel for Admin Tasks")]
        else:
            choices=choices1
        chosen_id = stx.tab_bar(data=choices, default=1)
        if int(chosen_id)==len(choices1)+1:
                task = st.selectbox("Task",["Add a new User","View all Users"])
                if task == "Add a new User":
                    st.subheader("Add a new User")
                    new_user = st.text_input("Username")
                    new_password = st.text_input("Password",type='password')

                    if st.button("Add"):
                        create_usertable()
                        add_userdata(new_user,make_hashes(new_password))
                        st.success("You have successfully created a valid Account")
                        st.info("This user can now access the app")

                elif task == "View all Users":
                    st.subheader("User Profiles")
                    user_result = view_all_users()
                    clean_db = pd.DataFrame(user_result,columns=["Username","Password"])
                    st.dataframe(clean_db)
        else:
            st.markdown("### Grid Eye")
            fig, ax = plt.subplots()
            ax = sns.heatmap(st.session_state.grid_eye)
            st.pyplot(fig)
            st.markdown("### Occupancy")
            fig, ax = plt.subplots()
            ax = plt.plot(st.session_state.hour, st.session_state.occupants)
            st.pyplot(fig)

if __name__ == '__main__':
    main()
