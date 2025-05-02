# MicroPython driver for LCA9555 16-channel I2C port expander
# Optimized with register caching and optional pre-warming

LCA9555_DEF_ADDRESS = const(0x20)

# Error codes
LCA9555_OK = const(0x00)
LCA9555_PIN_ERROR = const(0x81)
LCA9555_I2C_ERROR = const(0x82)
LCA9555_VALUE_ERROR = const(0x83)
LCA9555_INVALID_READ = const(-100)

# Register definitions
_INPUT_PORT_REGISTER_0       = const(0x00)
_INPUT_PORT_REGISTER_1       = const(0x01)
_OUTPUT_PORT_REGISTER_0      = const(0x02)
_OUTPUT_PORT_REGISTER_1      = const(0x03)
_POLARITY_REGISTER_0         = const(0x04)
_POLARITY_REGISTER_1         = const(0x05)
_CONFIGURATION_PORT_0        = const(0x06)
_CONFIGURATION_PORT_1        = const(0x07)

# Pin modes & values
INPUT  = const(1)
OUTPUT = const(0)
LOW    = const(0)
HIGH   = const(1)

class LCA9555:
    """
    MicroPython driver for the LCA9555 16-channel I2C port expander,
    with write caching to minimize I2C bus transactions and optional
    pre-warming of register cache at initialization.
    """
    def __init__(self, i2c, address=LCA9555_DEF_ADDRESS, pre_warm=False):
        self.i2c = i2c
        self.address = address
        self._error = LCA9555_OK
        # Cache for register values: reg_address->value
        self._reg_cache = {}
        self._pre_warm = pre_warm

    def begin(self):
        """Initialize device; optionally pre-warm cache."""
        ok = self.connected()
        if not ok:
            return False
        if self._pre_warm:
            # Read all config, polarity, and output registers
            for reg in (_INPUT_PORT_REGISTER_0, _INPUT_PORT_REGISTER_1,
                        _OUTPUT_PORT_REGISTER_0, _OUTPUT_PORT_REGISTER_1,
                        _POLARITY_REGISTER_0, _POLARITY_REGISTER_1,
                        _CONFIGURATION_PORT_0, _CONFIGURATION_PORT_1):
                try:
                    val = self._i2c_read_reg(reg)
                    self._reg_cache[reg] = val
                except OSError:
                    pass
        return True

    def connected(self):
        """Check device presence on I2C bus."""
        try:
            self.i2c.writeto(self.address, b'')
            self._error = LCA9555_OK
            return True
        except OSError:
            self._error = LCA9555_I2C_ERROR
            return False

    def last_error(self):
        return self._error

    def get_address(self):
        return self.address

    def _get_cached(self, reg):
        """Return cached value or read+cache if missing."""
        if reg not in self._reg_cache:
            try:
                val = self._i2c_read_reg(reg)
            except OSError:
                return None
            self._reg_cache[reg] = val
        return self._reg_cache[reg]

    def pin_mode(self, pin, mode, value=None):
        """
        Configure a pin as INPUT or OUTPUT.
        If setting to OUTPUT with optional initial value.
        """
        if not 0 <= pin < 16:
            self._error = LCA9555_PIN_ERROR
            return False
        if mode not in (INPUT, OUTPUT):
            self._error = LCA9555_VALUE_ERROR
            return False

        # Select configuration register and bit index
        if pin < 8:
            reg = _CONFIGURATION_PORT_0
            idx = pin
        else:
            reg = _CONFIGURATION_PORT_1
            idx = pin - 8

        prev = self._get_cached(reg)
        if prev is None:
            return False
        mask = 1 << idx
        cfg = prev | mask if mode == INPUT else prev & ~mask

        if cfg != prev:
            # If changing to OUTPUT with initial value, set output first
            if mode == OUTPUT and value is not None:
                if not self.write(pin, value):
                    return False
            if not self._i2c_write_reg(reg, cfg):
                return False
            self._reg_cache[reg] = cfg
        return True

    def write(self, pin, value):
        """Set a pin HIGH or LOW using cached register value."""
        if not 0 <= pin < 16:
            self._error = LCA9555_PIN_ERROR
            return False
        # Determine output register
        if pin < 8:
            reg = _OUTPUT_PORT_REGISTER_0
            idx = pin
        else:
            reg = _OUTPUT_PORT_REGISTER_1
            idx = pin - 8

        prev = self._get_cached(reg)
        if prev is None:
            return False
        mask = 1 << idx
        out = prev | mask if value else prev & ~mask

        if out != prev:
            if not self._i2c_write_reg(reg, out):
                return False
            self._reg_cache[reg] = out
        return True

    def read(self, pin):
        """Read pin level; does not use cache since inputs may change."""
        if not 0 <= pin < 16:
            self._error = LCA9555_PIN_ERROR
            return LCA9555_INVALID_READ
        if pin < 8:
            reg = _INPUT_PORT_REGISTER_0
            idx = pin
        else:
            reg = _INPUT_PORT_REGISTER_1
            idx = pin - 8
        try:
            val = self._i2c_read_reg(reg)
        except OSError:
            return LCA9555_INVALID_READ
        return HIGH if (val >> idx) & 1 else LOW

    def set_polarity(self, pin, polarity):
        """Invert input polarity for a pin."""
        if not 0 <= pin < 16:
            self._error = LCA9555_PIN_ERROR
            return False
        if polarity not in (HIGH, LOW):
            self._error = LCA9555_VALUE_ERROR
            return False
        if pin < 8:
            reg = _POLARITY_REGISTER_0
            idx = pin
        else:
            reg = _POLARITY_REGISTER_1
            idx = pin - 8

        prev = self._get_cached(reg)
        if prev is None:
            return False
        mask = 1 << idx
        pol = prev | mask if polarity else prev & ~mask

        if pol != prev:
            if not self._i2c_write_reg(reg, pol):
                return False
            self._reg_cache[reg] = pol
        return True

    def get_polarity(self, pin):
        """Get input polarity inversion state for a pin."""
        if not 0 <= pin < 16:
            self._error = LCA9555_PIN_ERROR
            return None
        if pin < 8:
            reg = _POLARITY_REGISTER_0
            idx = pin
        else:
            reg = _POLARITY_REGISTER_1
            idx = pin - 8
        try:
            pol = self._i2c_read_reg(reg)
        except OSError:
            return None
        return HIGH if (pol >> idx) & 1 else LOW

    def _i2c_write_reg(self, reg, value):
        """Write a byte to a register over I2C."""
        try:
            self.i2c.writeto(self.address, bytes([reg, value]))
            self._error = LCA9555_OK
            return True
        except OSError:
            self._error = LCA9555_I2C_ERROR
            return False

    def _i2c_read_reg(self, reg):
        """Read a byte from a register over I2C."""
        self.i2c.writeto(self.address, bytes([reg]), False)
        data = self.i2c.readfrom(self.address, 1)
        self._error = LCA9555_OK
        return data[0]
