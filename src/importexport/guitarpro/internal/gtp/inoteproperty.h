#ifndef MU_IMPORTEXPORT_INOTEPROPERTY_H
#define MU_IMPORTEXPORT_INOTEPROPERTY_H

namespace mu::iex::guitarpro {
class INoteProperty {
public:
    INoteProperty() = default;
    virtual ~INoteProperty() = default;
};

class BendProperty : public INoteProperty {
public:

    void set_destination_offset(float v) { destination_offset_value = v; }

    void set_destination_value(float v) { destination_value_value = v; }

    void set_middle_offset1(float v) { middle_offset1_value = v; }

    void set_middle_offset2(float v) { middle_offset2_value = v; }

    void set_middle_value(float v) { middle_value_value = v; }

    void set_origin_offset(float v) { origin_offset_value = v; }

    void set_origin_value(float v) { origin_value_value = v; }

    float destination_offset() const { return destination_offset_value; }

    float destination_value() const { return destination_value_value; }

    float middle_offset1() const { return middle_offset1_value; }

    float middle_offset2() const { return middle_offset2_value; }

    float middle_value() const { return middle_value_value; }

    float origin_offset() const { return origin_offset_value; }

    float origin_value() const { return origin_value_value; }

private:
    float destination_offset_value{-1};
    float destination_value_value{-1};
    float middle_offset1_value{-1};
    float middle_offset2_value{-1};
    float middle_value_value{-1};
    float origin_offset_value{-1};
    float origin_value_value{-1};
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_INOTEPROPERTY_H
