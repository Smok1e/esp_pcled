function update(time)
    ledstrip.clear()
    ledstrip.setPixelRGB(
        math.floor(ledstrip.numLeds * time / 3) % ledstrip.numLeds + 1,
        0xFF,
        0x00,
        0x00
    )
end