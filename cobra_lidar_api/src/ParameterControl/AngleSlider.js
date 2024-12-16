import React, {Component} from "react";
import Nouislider from "nouislider-react";
import "nouislider/distribute/nouislider.css"
import "./AngleSlider.scss";


class AngleSlider extends Component {
    onSlide(value) {
        const angles = [parseInt(value[0]), parseInt(value[1])]
        this.props.onSlide(angles);
    }

    render() {
        let className;
        switch (this.props.color) {
            case ("blue"):
            case ("yellow"):
            case ("red"):
            case ("green"):
            case ("grey"):
                className = this.props.color;
                break;
            default:
                className = "blue";
        }

        return (
            <div className={`pb-3 pt-5 ${this.props.hidden ? "hidden" : ""}`}>
                <Nouislider
                    range={{
                        min: [this.props.min || this.props.low || -45],
                        max: [this.props.max || this.props.high || 45],
                    }}
                    start={this.props.start || [-30, 30]}
                    value={this.props.value}
                    connect={[false, true, false]}
                    onSlide={(render, handle, value) => this.onSlide(value)}
                    tooltips={[true, true]}
                    behaviour={"tap-drag"}
                    className={className}
                    format={{
                        to: (val) => `${parseInt(val)}\u00B0`,
                        from: (val) => parseInt(val.replace("\u00B0", "")),
                    }}
                />
            </div>
        );
    }
}

export default AngleSlider;
