from onem2mlib import *
import onem2mlib.notifications as NOT
import onem2mlib.constants as CON
import hashlib
import sqlite3

import matplotlib.pylab as plt
import extra_streamlit_components as stx
import pandas as pd
import streamlit as st

PAGE_CONFIG = {'page_title': 'ESW Project PIR Demo', 'layout': "wide"}
st.set_page_config(**PAGE_CONFIG)


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


def callback(resource):
    global session_id
    print(resource.content)
    rerun(session_id=session_id)


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
        st.session_state.session = Session(
            'http://0.0.0.0:8080/', 'Cae-admin')
        st.session_state.cse = CSEBase(st.session_state.session, 'server')
        st.session_state.ae = AE(
            st.session_state.cse, resourceName='gokul')
        st.session_state.pir = st.session_state.ae.containers()[0]
        st.session_state.notifs = NOT.setupNotifications(callback)
        st.session_state.pir_subscription = st.session_state.pir.subscribe()
    st.session_state.y = [int(i) for i in st.session_state.pir.contents()]
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
                fig.patch.set_facecolor('#eadedc')
                ax = plt.plot(st.session_state.x, st.session_state.y)
                st.pyplot(fig)
            with c3:
                pass


if __name__ == '__main__':
    main()
