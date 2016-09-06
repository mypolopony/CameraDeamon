.PHONY: clean All

All:
	@echo "----------Building project:[ CameraDeamon - Debug ]----------"
	@cd "CameraDeamon" && "$(MAKE)" -f  "CameraDeamon.mk"
clean:
	@echo "----------Cleaning project:[ CameraDeamon - Debug ]----------"
	@cd "CameraDeamon" && "$(MAKE)" -f  "CameraDeamon.mk" clean
