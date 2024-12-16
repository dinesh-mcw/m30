import React, {Component} from "react";
import Navbar from "react-bootstrap/Navbar";
import {Container, Nav, NavDropdown} from "react-bootstrap";
import globalVariables from "../Common/globalVariables";
import Logo from "./lumotive-logo-horiz-color.png";

const {baseUrl} = globalVariables;

// System state links
const updateUrl = `${baseUrl}/update`;
const restartUrl = `${baseUrl}/restart`;


// Download links
const logsUrl = `${baseUrl}/logs`;
const mappingUrl = `${baseUrl}/mapping`;

class NavigationBar extends Component {
    post(url, doneCallback, failCallback) {
        fetch(url, {method: "POST"})
            .then(response => {
                if (response.ok) {
                    return response.json()
                }
                throw response;
            })
            .then(doneCallback)
            .catch(failCallback)
    }

    update() {
        const msg1 = "Please confirm restart to update/recovery mode."
        const msg2 = "Update/recovery mode may take up to 2 minutes to start. Are you sure you want to proceed?"
        if ((window.confirm(msg1) && window.confirm(msg2))) {
            fetch(updateUrl, {method: "POST"})
                .catch(e => console.error(e))

            const sleepSeconds = 90;  // Delay before redirecting
            window.setTimeout(
                () => {
                    // Get the base URL
                    const location = window.location;
                    const base = location.protocol + "//" + location.host;
                    const cidx = base.lastIndexOf(":")
                    // Strip the existing port if it exists, then add the new one
                    window.location.href = (window.location.port ? base.substring(0, cidx) : base) + ":8080";
                },
                sleepSeconds * 1000,
            )
        }
    }

    render() {
        // Img href
        const imgUrl = "#";

        return (
                <Navbar bg={"light"} expand={"lg"} sticky={"top"}>
                <Container>
                    <Navbar.Brand href={imgUrl}>
                        <img
                            src={Logo}
                            alt={"Lumotive Logo"}
                            width={300}
                            height={32}
                            className={"img-fluid px-5"}

                        />
                    </Navbar.Brand>

                    <Navbar.Toggle aria-controls={"nav-collapse"}/>
                    <Navbar.Collapse id={"nav-collapse"}>
                        <Nav>
                            <NavDropdown title={"    Downloads    "} style={{width: 200}}
                                id={"downloads-dropdowns"}>
                                <NavDropdown.Item
                                    href={logsUrl}
                                    download={"m30_logs.zip"}
                                    target={"_blank"}
                                >
                                    System Logs
                                </NavDropdown.Item>
                                <NavDropdown.Item
                                    href={mappingUrl}
                                    download={"mapping_tables.zip"}
                                    target={"_blank"}
                                >
                                    Mapping Tables
                                </NavDropdown.Item>
                            </NavDropdown>
                            <Nav.Link
                                id={"update-link"}style={{width: 200}}
                                onClick={() => this.update()}
                            >Software Update</Nav.Link>
                            <Nav.Link
                                id={"restart-link"}
            style={{width: 200}}
                                onClick={() => this.post(restartUrl)}
                            >Restart Sensor Head</Nav.Link>
                            {/*<Nav.Link
                                id={"shutdown-link"}
                                onClick={() => this.post(disableUrl)}
                            >
                                Shutdown
                                </Nav.Link>*/}
                        </Nav>
                    </Navbar.Collapse>
                </Container>
            </Navbar>
        );
    }
}

export default NavigationBar;
