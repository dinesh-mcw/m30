import React, {useState, useEffect} from "react";
import globalVariables from "./Common/globalVariables";
import LogBox from "./LogBox/LogBox";
import IntervalFetch from "./Common/IntervalFetch.js";
import Stack from "react-bootstrap/Stack";

const BASE_URL = globalVariables.baseUrl;

const DiagnosticLog = props => {
    const url = `${BASE_URL}/messages`;
    const [messages, setMessages] = useState([]);
    const {maxMessages=1000} = props;

    // Simple way - poll a message endpoint

    useEffect(() => {
        const fetcher = new IntervalFetch(url, {}, async (data) => {
            if (data.length > 0) {
                // Load in the new messages, keeping only the last few lines
                const newMessages = [...messages, data].slice(-maxMessages)
                setMessages(newMessages)
            }
        });
        fetcher.start(1000, 10000);
        return () => fetcher.stop();
    }, [url, messages, maxMessages]);


    return (
        <Stack gap={3}>
            <div className={"h4 text-center"}>Diagnostic Log</div>
            <LogBox messages={messages} maxMessages={maxMessages} />
        </Stack>
    )
}

export default DiagnosticLog;
