import {Component} from "react";
import "./LandingPage.css";
import LogBox from "../LogBox/LogBox";
import {Container} from "react-bootstrap";
import Row from "react-bootstrap/Row";

import Logo from "./Lumotive-logo_vertical_full-color_dark-text_large.png";


class LandingPage extends Component {
    render() {
        const anchorProps = {
            href: "https://lumotive.com",
            target: "_blank",  // New tab/window
            rel: "noopener noreferrer",
        };

        const msg = "Checking for sensor head updates...";
        const {messages=[], maxMessages} = this.props;
        return (
            <Container className={"Landing"}>
                <Row className={"py-4"}>
                    <a {...anchorProps}>
                <img src={Logo} alt={"logo"} width={1080/2} height={420/2} />
                    </a>
                </Row>
                <Row className={"py-4"}>
                    <LogBox
                        messages={[msg, ...messages]}
                        maxMessages={maxMessages}
                    />
                </Row>
            </Container>
        )
    }
}

export default LandingPage;
