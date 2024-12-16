import React from "react";
import {Arc, Image, Layer, Stage} from "react-konva";
import useImage from "use-image";
import variables from "../custom.scss";

import LidarImg from "./m30_lidar_side_view.png";

const IMGWIDTH = 560, IMGHEIGHT = 1035;
const LIDARASPECT = IMGWIDTH / IMGHEIGHT;

const LIDARHEIGHTFRAC = 0.9;  // fraction [0, 1] of lidar height on canvas


function Lidar(props) {
    const [image] = useImage(LidarImg)

    function getImageProps() {
        const {width, height} = props;

        const imgHeight = height * LIDARHEIGHTFRAC;
        const imgWidth = imgHeight * LIDARASPECT;

        // Center the sensor on-screen
        const offsetX = -width / 2;
        const offsetY = (imgHeight - height) / 2;

        return {
            height: imgHeight,
            width: imgWidth,
            offsetX: offsetX,
            offsetY: offsetY,
        }
    }

    return <Image image={image} {...getImageProps()} />;
}


function VirtualSensor(props) {
    function getArcProps() {
        const {angleRange, color, width, height} = props;

        const angle = Math.max(...angleRange) - Math.min(...angleRange);
        const rotation = Math.min(...angleRange) + 180;

        let virtualSensorRadius;

        if (width < height) {
            virtualSensorRadius = width / 2;
        } else {
            virtualSensorRadius = height / Math.sqrt(2);
        }

        // Put at center of canvas - approx. where LCM would be
        const virtualSensorX = width / 2;
        const virtualSensorY = height / 2;

        // For gradient color
        const colorStops = [
            0.0, color,
            0.4, color,
            1, "white",
        ]

        return {
            x: virtualSensorX,
            y: virtualSensorY,
            innerRadius: 0,  // Full arc
            outerRadius: virtualSensorRadius,
            angle: angle,
            rotation: rotation,
            opacity: 0.8,  // Allow colors to overlap
            fillRadialGradientStartRadius: 0,
            fillRadialGradientEndRadius: virtualSensorRadius,
            fillRadialGradientColorStops: colorStops,
        }
    }

    try {
        return <Arc {...getArcProps()} />
    } catch (Error) {
        return <></>
    }
}


function ScanCanvas(props, ref) {
    const divRef = ref;
    const [size, setSize] = React.useState({
        width: props.width || 300,
        height: props.height || 200,
    })
    const {angleRanges} = props;
    let virtualSensorInfo = [];

    let colors = [variables.lumoGreen, variables.lumoYellow, variables.lumoRed];

    angleRanges.forEach((virtualSensor, idx) => {
        virtualSensorInfo.push({
            angleRange: virtualSensor,
            color: colors[idx % colors.length],
        })
    })

    React.useEffect(() => {
        function checkSize() {
            try {
                setSize({
                    width: divRef.current.clientWidth || 300,
                    height: divRef.current.clientHeight || 200,
                })
            } catch (TypeError) {
                // Do nothing
            }
        }

        window.addEventListener("resize", checkSize);
        return () => window.removeEventListener("resize", checkSize)
    }, [divRef]);


    return (
        <Stage width={size.width} height={size.height}>
            <Layer>
                <Lidar width={size.width} height={size.height}/>
            </Layer>
            <Layer>
                {virtualSensorInfo.map((info, idx) => <VirtualSensor
                    key={idx}
                    width={size.width}
                    height={size.height}
                    {...info}
                />)}
            </Layer>
        </Stage>
    );
}

export default React.forwardRef(ScanCanvas);
