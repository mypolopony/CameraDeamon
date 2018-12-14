.PHONY: clean All

All:
	@echo "----------Building project:[ CameraDeamon - Release ]----------"
	@cd "CameraDeamon" && "$(MAKE)" -f  "CameraDeamon.mk"
clean:
	@echo "----------Cleaning project:[ CameraDeamon - Release ]----------"
	@cd "CameraDeamon" && "$(MAKE)" -f  "CameraDeamon.mk" clean
