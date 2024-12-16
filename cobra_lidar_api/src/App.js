import React, {Component} from "react";
import NavigationBar from "./Navbar/NavigationBar";
import ControlPanel from "./ControlPanel";
import "./custom.scss";

import globals from "./Common/globalVariables";
import LandingPage from "./LandingPage/LandingPage";
import Footer from "./Footer/footer";
import IntervalFetch from "./Common/IntervalFetch";

const {baseUrl} = globals;

const READY_MSG = "System Bootup Complete"


class App extends Component {
    constructor(props) {
        super(props);

        var _this = this;
        this.fetcher = new IntervalFetch(`${baseUrl}/messages/update`, { useText: true }, async (data) => _this.handleMsg(data) );
        this.state = {
            messages: [],
            maxMessages: 1000,
        }
    }

    get ready() {
        return this.lastMsg === READY_MSG
    }

    get lastMsg() {
        return this.state.messages.slice(-1)[0]
    }

    handleMsg(data) {
        if (data !== this.lastMsg) {
            const newMessages = [...this.state.messages, data];
            const newState = {
                messages: newMessages.slice(-this.state.maxMessages),
            }

            this.setState(newState,() => this.validateMsg());
        }
    }

    validateMsg() {
        if (this.ready) {
            this.fetcher.stop();
        }
    }

    componentDidMount() {
        if (!this.ready) {
            this.fetcher.start(0, 3000);
        }
    }

    componentWillUnmount() {
        this.fetcher.stop();
    }

    render() {
        if (this.ready) {
            return (
                <div className={"wrapper"}>
                    <NavigationBar/>
                    <ControlPanel />
                    <Footer />
                </div>
            );
        } else {
            return (
                <LandingPage
                    messages={this.state.messages}
                    maxMessages={this.state.maxMessages}
                />
            )
        }
    }
}

export default App;
