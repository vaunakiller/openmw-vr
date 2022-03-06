---
-- `openmw.camera` controls camera.
-- Can be used only by player scripts.
-- @module camera
-- @usage local camera = require('openmw.camera')


---
-- @type MODE Camera modes.
-- @field #number Static Camera doesn't track player; player inputs doesn't affect camera; use `setStaticPosition` to move the camera.
-- @field #number FirstPerson First person mode.
-- @field #number ThirdPerson Third person mode; player character turns to the view direction.
-- @field #number Vanity Similar to Preview; camera slowly moves around the player.
-- @field #number Preview Third person mode, but player character doesn't turn to the view direction.

---
-- Camera modes.
-- @field [parent=#camera] #MODE MODE

---
-- Return the current @{openmw.camera#MODE}.
-- @function [parent=#camera] getMode
-- @return #MODE

---
-- Return the mode the camera will switch to after the end of the current animation. Can be nil.
-- @function [parent=#camera] getQueuedMode
-- @return #MODE

---
-- Change @{openmw.camera#MODE}; if the second (optional, true by default) argument is set to false, the switching can be delayed (see `getQueuedMode`).
-- @function [parent=#camera] setMode
-- @param #MODE mode
-- @param #boolean force

---
-- If set to true then after switching from Preview to ThirdPerson the player character turns to the camera view direction. Otherwise the camera turns to the character view direction.
-- @function [parent=#camera] allowCharacterDeferredRotation
-- @param #boolean boolValue

---
-- Show/hide crosshair.
-- @function [parent=#camera] showCrosshair
-- @param #boolean boolValue

---
-- Current position of the tracked object (the characters head if there is no animation).
-- @function [parent=#camera] getTrackedPosition
-- @return openmw.util#Vector3

---
-- Current position of the camera.
-- @function [parent=#camera] getPosition
-- @return openmw.util#Vector3

---
-- Camera pitch angle (radians) without taking extraPitch into account.
-- Full pitch is `getPitch()+getExtraPitch()`.
-- @function [parent=#camera] getPitch
-- @return #number

---
-- Force the pitch angle to the given value (radians); player input on this axis is ignored in this frame.
-- @function [parent=#camera] setPitch
-- @param #number value

---
-- Camera yaw angle (radians) without taking extraYaw into account.
-- Full yaw is `getYaw()+getExtraYaw()`.
-- @function [parent=#camera] getYaw
-- @return #number

---
-- Force the yaw angle to the given value (radians); player input on this axis is ignored in this frame.
-- @function [parent=#camera] setYaw
-- @param #number value

---
-- Get camera roll angle (radians).
-- @function [parent=#camera] getRoll
-- @return #number

---
-- Set camera roll angle (radians).
-- @function [parent=#camera] setRoll
-- @param #number value

---
-- Additional summand for the pitch angle that is not affected by player input.
-- Full pitch is `getPitch()+getExtraPitch()`.
-- @function [parent=#camera] getExtraPitch
-- @return #number

---
-- Additional summand for the pitch angle; useful for camera shaking effects.
-- Setting extra pitch doesn't block player input.
-- Full pitch is `getPitch()+getExtraPitch()`.
-- @function [parent=#camera] setExtraPitch
-- @param #number value

---
-- Additional summand for the yaw angle that is not affected by player input.
-- Full yaw is `getYaw()+getExtraYaw()`.
-- @function [parent=#camera] getExtraYaw
-- @return #number

---
-- Additional summand for the yaw angle; useful for camera shaking effects.
-- Setting extra pitch doesn't block player input.
-- Full yaw is `getYaw()+getExtraYaw()`.
-- @function [parent=#camera] setExtraYaw
-- @param #number value

---
-- Set camera position; can be used only if camera is in Static mode.
-- @function [parent=#camera] setStaticPosition
-- @param openmw.util#Vector3 pos

---
-- The offset between the characters head and the camera in first person mode (3d vector).
-- @function [parent=#camera] getFirstPersonOffset
-- @return openmw.util#Vector3

---
-- Set the offset between the characters head and the camera in first person mode (3d vector).
-- @function [parent=#camera] setFirstPersonOffset
-- @param openmw.util#Vector3 offset

---
-- Preferred offset between tracked position (see `getTrackedPosition`) and the camera focal point (the center of the screen) in third person mode.
-- See `setFocalPreferredOffset`.
-- @function [parent=#camera] getFocalPreferredOffset
-- @return openmw.util#Vector2

---
-- Set preferred offset between tracked position (see `getTrackedPosition`) and the camera focal point (the center of the screen) in third person mode.
-- The offset is a 2d vector (X, Y) where X is horizontal (to the right from the character) and Y component is vertical (upward).
-- The real offset can differ from the preferred one during smooth transition of if blocked by an obstacle.
-- Smooth transition happens by default every time when the preferred offset was changed. Use `instantTransition()` to skip the current transition.
-- @function [parent=#camera] setFocalPreferredOffset
-- @param openmw.util#Vector2 offset

---
-- The actual distance between the camera and the character in third person mode; can differ from the preferred one if there is an obstacle.
-- @function [parent=#camera] getThirdPersonDistance
-- @return #number

---
-- Set preferred distance between the camera and the character in third person mode.
-- @function [parent=#camera] setPreferredThirdPersonDistance
-- @param #number distance

---
-- The current speed coefficient of focal point (the center of the screen in third person mode) smooth transition.
-- @function [parent=#camera] getFocalTransitionSpeed
-- @return #number

---
-- Set the speed coefficient of focal point (the center of the screen in third person mode) smooth transition.
-- Smooth transition happens by default every time when the preferred offset was changed. Use `instantTransition()` to skip the current transition.
-- @function [parent=#camera] setFocalTransitionSpeed
-- Set the speed coefficient 
-- @param #number speed

---
-- Make instant the current transition of camera focal point and the current deferred rotation (see `allowCharacterDeferredRotation`).
-- @function [parent=#camera] instantTransition


return nil

